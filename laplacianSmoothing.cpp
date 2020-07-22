////////////////////////////////////////////////////////////////////////
// DESCRIPTION:
//
// LAPLACIAN SMOOTHING
//
// To use this node: 
//	(1) Select the object. 
//	(2) Type: "deformer -type laplacianSmoothing". 
//	(3) Bring up the channel box. 
//	(4) Select the laplacianSmoothing input. 
//	(5) Change the Iterations value of the laplacianSmoothing input in the channel box. 
////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <math.h>
#include <map>

#include <maya/MIOStream.h>
#include <maya/MPxGeometryFilter.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshVertex.h>

#include <maya/MTypeId.h> 
#include <maya/MPlug.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnDependencyNode.h>

#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MMatrix.h>

#define McheckErr(stat, msg)		\
		if ( MS::kSuccess != stat )	\
		{							\
			cerr << msg;			\
			return MS::kFailure;	\
		}

double getWeight(const MPoint &, const MPoint &, const MPoint &, const MPoint &);
MPoint L(const MIntArray &, std::map <int, MPoint> &, const MPoint &);

class laplacianSmoothing : public MPxGeometryFilter
{
public:
	laplacianSmoothing();
	~laplacianSmoothing() override;

	static void		*creator();
	static MStatus	initialize();

    MStatus deform (MDataBlock		&block,
					MItGeometry 	&iter,
					const MMatrix 	&mat,
					unsigned int 	multiIndex) override;

public:

	static MObject op;
	static MObject it;
	static MObject coef;
	static MObject coef_taub;
	static MTypeId id;
	static MString name;

private:

};

MTypeId laplacianSmoothing::id(0x8000e);
MString laplacianSmoothing::name("laplacianSmoothing");

// laplacianSmoothing attributes
MObject laplacianSmoothing::op;
MObject laplacianSmoothing::it;
MObject laplacianSmoothing::coef;
MObject laplacianSmoothing::coef_taub;


laplacianSmoothing::laplacianSmoothing()
{}

laplacianSmoothing::~laplacianSmoothing()
{}

void *laplacianSmoothing::creator()
{
	return new laplacianSmoothing();
}

MStatus laplacianSmoothing::initialize()
{
	// local attribute initialization
	MFnEnumAttribute eAttr;
	op =	eAttr.create("operation", "op");
			eAttr.addField("Laplace", 0);
			eAttr.addField("Taubin", 1);
			eAttr.setKeyable(true);

	MFnNumericAttribute nAttr;
	it =	nAttr.create("iterations", "it", MFnNumericData::kInt);
			nAttr.setDefault(0);
			nAttr.setMin(0);
			nAttr.setKeyable(true);
	coef =	nAttr.create("coefficient", "coef", MFnNumericData::kDouble);
			nAttr.setDefault(0.33);
			nAttr.setMin(0.0);
			nAttr.setMax(1.0);
			nAttr.setKeyable(true);
	coef_taub = nAttr.create("Taubin coefficient", "Tcoef", MFnNumericData::kDouble);
			nAttr.setDefault(-0.34);
			nAttr.setMin(-1.0);
			nAttr.setMax(0.0);
			nAttr.setKeyable(true);

	addAttribute(op);
	addAttribute(it);
	addAttribute(coef);
	addAttribute(coef_taub);

	// affects
	attributeAffects(laplacianSmoothing::op, laplacianSmoothing::outputGeom);
    attributeAffects(laplacianSmoothing::it, laplacianSmoothing::outputGeom);
	attributeAffects(laplacianSmoothing::coef, laplacianSmoothing::outputGeom);
	attributeAffects(laplacianSmoothing::coef_taub, laplacianSmoothing::outputGeom);

	return MS::kSuccess;
}

MStatus laplacianSmoothing::deform (MDataBlock		&block,
									MItGeometry		&iter,
									const MMatrix	&/*mat*/,
									unsigned int	multiIndex)
// Description:   Deform the point with a laplacianSmoothing algorithm
//
// Arguments:
//   block		: the datablock of the node
//	 iter		: an iterator for the geometry to be deformed
//   m    		: matrix to transform the point into world space
//	 multiIndex : the index of the geometry that we are deforming
{
	MStatus status = MS::kSuccess;
	
	// Тип операции (Лаплас, Таубин)
	int opData = block.inputValue(op, &status).asShort();
	McheckErr(status, "Error getting operation data handle\n");
	// Кол-во итераций
	int iterData = block.inputValue(it, &status).asInt();
	McheckErr(status, "Error getting iterations data handle\n");
	// Коэффициент альфа
	double coefData = block.inputValue(coef, &status).asDouble();
	McheckErr(status, "Error getting coefficient data handle\n");
	// Коэффициент для сглаживания Таубина - мю
	double coefTaubData = block.inputValue(coef_taub, &status).asDouble();
	McheckErr(status, "Error getting Taubin coefficient data handle\n");
	float env = block.inputValue(envelope, &status).asFloat();
	McheckErr(status, "Error getting envelope data handle\n");

	MArrayDataHandle inputData = block.inputArrayValue(input, &status);
	McheckErr(status, "Error getting input data handle\n");
	inputData.jumpToElement(multiIndex);
	MDataHandle inputGeomData = inputData.inputValue(&status).child(inputGeom);

	int index;
	unsigned int i, j;
	MItMeshVertex vertexIt(inputGeomData.asMesh(), &status);
	MIntArray connected;
	MPointArray newPts;
	std::map <int, MPoint> oldPts;

	for (iter.reset(); !iter.isDone(); iter.next())
	{
		oldPts[iter.index()] = iter.position();
		newPts.append(iter.position());
	}

	// iterate through each point in the geometry
	//
	for (unsigned int m = 0; m < iterData * env; m++)
	{
		for (iter.reset(), vertexIt.reset(), j = 0; !iter.isDone(); iter.next(), vertexIt.next(), j++)
		{
			vertexIt.setIndex(iter.index(), index);

			// Закрепление границы
			if (vertexIt.onBoundary())
				continue;

			// Индексы соседних вершин
			vertexIt.getConnectedVertices(connected);
			MPoint pt = oldPts[iter.index()], tmp = L(connected, oldPts, pt), delta(0.0, 0.0);

			// Расчёт новой позиции
			if (tmp == delta) continue;
			pt = pt + coefData * tmp;

			// Доп. итерация для сглаживания Таубина
			if (opData == 1)
			{
				tmp = L(connected, oldPts, pt);
				if (tmp == delta) continue;
				pt = pt + coefTaubData * tmp;
			}

			newPts[j] = pt;
		}

		for (iter.reset(), i = 0; !iter.isDone(); iter.next(), i++)
			oldPts[iter.index()] = newPts[i];
	}

	iter.setAllPositions(newPts);

	return status;
}

// Расчёт оператора Лапласа
MPoint L(const MIntArray &connected, std::map <int, MPoint> &oldPts, const MPoint &pt)
{
	MPoint L(0.0, 0.0), delta(0.0, 0.0);
	unsigned int i;
	double w, w_ij;

	for (i = 0, w = 0; i < connected.length(); i++, w += w_ij)
	{
		// Если соседняя вершина не принадлежит выделенной области
		if (oldPts.find(connected[i]) == oldPts.end()) break;

		int next = (i == connected.length() - 1) ? 0 : i + 1;
		int prev = (i == 0) ? connected.length() - 1 : i - 1;

		w_ij = getWeight(pt, oldPts[connected[i]], oldPts[connected[next]], oldPts[connected[prev]]);

		if (isnan(w_ij)) w_ij = 0;

		const float ctg_max = cos(1e-6f) / sin(1e-6f);
		if (w_ij >= ctg_max) w_ij = ctg_max;
		//if (isinf(w_ij)) w_ij = cos(1e-6f) / sin(1e-6f);

		L = L + oldPts[connected[i]] * w_ij;
	}
	if (i != connected.length()) return delta;

	L = L / w - pt;

	return L;
}

// Расчёт cotangent weights
double getWeight(const MPoint &i, const MPoint &j, const MPoint &p1, const MPoint &p2)
{
	double a_ij, b_ip1, c_jp1, d_ip2, e_jp2;

	a_ij = i.distanceTo(j);
	b_ip1 = i.distanceTo(p1);
	c_jp1 = j.distanceTo(p1);
	d_ip2 = i.distanceTo(p2);
	e_jp2 = j.distanceTo(p2);

	double cos_A, cos_B, ctg_A, ctg_B;

	cos_A = (pow(b_ip1, 2) + pow(c_jp1, 2) - pow(a_ij, 2)) / (2 * b_ip1 * c_jp1);
	cos_B = (pow(d_ip2, 2) + pow(e_jp2, 2) - pow(a_ij, 2)) / (2 * d_ip2 * e_jp2);

	ctg_A = cos_A / sqrt(1 - pow(cos_A, 2));
	ctg_B = cos_B / sqrt(1 - pow(cos_B, 2));

	return (ctg_A + ctg_B) / 2;
}

// standard initialization procedures
MStatus initializePlugin(MObject obj)
{
	MStatus result;
	MFnPlugin plugin(obj, PLUGIN_COMPANY, "1.0", "Any");
	result = plugin.registerNode(laplacianSmoothing::name, laplacianSmoothing::id, laplacianSmoothing::creator,
								 laplacianSmoothing::initialize, MPxNode::kDeformerNode);
	return result;
}

MStatus uninitializePlugin(MObject obj)
{
	MStatus result;
	MFnPlugin plugin(obj);
	result = plugin.deregisterNode(laplacianSmoothing::id);
	return result;
}
