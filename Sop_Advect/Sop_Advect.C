
#include "Sop_Advect.h"

#include <limits.h>
#include <SYS/SYS_Math.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Interrupt.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <GU/GU_Detail.h>
#include <GEO/GEO_PrimPoly.h>
#include <PRM/PRM_Include.h>
#include <CH/CH_LocalVariable.h>
#include <OP/OP_AutoLockInputs.h>
#include <GU/GU_PrimVDB.h>

#include <UT/UT_DSOVersion.h>
#include <UT/UT_Interrupt.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <GEO/GEO_Detail.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVolume.h>
#include <CH/CH_Manager.h>
#include "..\..\test\ParmFactory.h"
 
// Include Standard C Package
#include <stdio.h>
#include <stdlib.h>

using namespace std;
using namespace M_VolAdvect;
 


//----------------------------------------------------
// REGISTER
// register the operator in Houdini, it is a hook ref for Houdini


// create PRM_ChoiceList namedVolumesGlobMenu for input 2
static void sopBuildRestGroup(void *data, PRM_Name *choicenames, int listsize,
	const PRM_SpareData *spare, const PRM_Parm *parm)
{
	SOP_Node    *sop = CAST_SOPNODE((OP_Node *)data);
	if (sop)
	{
		sop->buildNamedPrims(1, choicenames, listsize, parm,true,true);
	}
	else
	{
		choicenames[0].setToken(0);
		choicenames[0].setLabel(0);
	}
}

static PRM_ChoiceList   namedVolumesGlobMenu2(PRM_CHOICELIST_TOGGLE, sopBuildRestGroup);
 
void newSopOperator(OP_OperatorTable *table)
{

	houdini_utils::ParmList parms;

	// Define a string-valued group name pattern parameter and add it to the list.
	parms.add(houdini_utils::ParmFactory(PRM_STRING, "volume_to_advect", "volume to advect")
		.setHelpText("Specify the volume to advect")
		.setChoiceList(&SOP_Node::namedVolumesGlobMenu));
	// Define a string-valued group name pattern parameter and add it to the list.

	parms.add(houdini_utils::ParmFactory(PRM_STRING, "velocity_volume", "velocity volume")
		.setHelpText("Specify the velocity volume")
		.setChoiceList(&namedVolumesGlobMenu2));
	
	// amplitude
	parms.add(houdini_utils::ParmFactory(PRM_FLT_J, "amp", "Amplitude")
		.setDefault(PRMoneDefaults)
		.setRange(PRM_RANGE_RESTRICTED, 0.0, PRM_RANGE_FREE, 10.0));
	 
	parms.add(houdini_utils::ParmFactory(PRM_INT_J, "substep", "substep")
		.setDefault(1)
		.setRange(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_FREE, 10.0));

    OP_Operator *op;

	op = new OP_Operator(
    		"volume_advect",                  // internal name, needs to be unique in OP_OperatorTable (table containing all nodes for a network type - SOPs in our case, each entry in the table is an object of class OP_Operator which basically defines everything Houdini requires in order to create nodes of the new type)
    		"Volume Advect",                  // UI name
    		SOP_VolAdvect::myConstructor,     // how to build the node - A class factory function which constructs nodes of this type
			parms.get(),					 // my parameters - An array of PRM_Template objects defining the parameters to this operator
    		2,                                // min # of sources
    		2);                               // max # of sources

    // place this operator under the Volume submenu in the TAB menu.
    op->setOpTabSubMenuPath("Volume");

    // after addOperator(), 'table' will take ownership of 'op'
    table->addOperator(op);
} 


// label node inputs, 0 corresponds to first input, 1 to the second one
const char *
SOP_VolAdvect::inputLabel(unsigned idx) const
{
    switch (idx){
        case 0: return "Volume density";
        case 1: return "Volume Velocity";
        default: return "default";
    }
}

//--------------------------------------------------
// UI
// define parameter for debug option
static PRM_Name debugPRM("debug", "Print debug information"); // internal name, UI name
 

// assign parameter to the interface, which is array of PRM_Template objects
PRM_Template SOP_VolAdvect::myTemplateList[] = 
{
    PRM_Template(PRM_TOGGLE, 1, &debugPRM, PRMzeroDefaults), // type (checkbox), size (one in our case, but rgb/xyz values would need 3), pointer to a PRM_Name describing the parameter name, default value (0 - disabled)
	 
	PRM_Template() // at the end there needs to be one empty PRM_Template object
};



//----------------------------------------------
// Constructors
// constructors, destructors, usually there is no need to really modify anything here, the constructor's job is to ensure the node is put into the proper network
OP_Node * 
SOP_VolAdvect::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new SOP_VolAdvect(net, name, op);
}

SOP_VolAdvect::SOP_VolAdvect(OP_Network *net, const char *name, OP_Operator *op) : SOP_Node(net, name, op) {}

SOP_VolAdvect::~SOP_VolAdvect() {}


//---------------------------------------------
 

OP_ERROR SOP_VolAdvect::cookMySop(OP_Context &context)
{
	 cout << "start cooking" << endl;
	 float   t = context.getTime();
    float amp = evalFloat("amp", 0, t);
	float sub = evalFloat("substep", 0, t);
	// We must lock our inputs before we try to access their geometry.
	// OP_AutoLockInputs will automatically unlock our inputs when we return.
	// NOTE: Don't call unlockInputs yourself when using this!
 
	 OP_AutoLockInputs inputs(this);
	 if (inputs.lock(context) >= UT_ERROR_ABORT)
		 return error();

	 // Duplicate input geometry

	 duplicateSource(0, context, gdp);

	 const GU_Detail * second_input =inputGeo(1); 
	 //duplicateSource(1, context, second_input);

	 flags().setTimeDep(true);
 
	 // read input
	 UT_String vol_advect_name;
	 evalString(vol_advect_name, "volume_to_advect", 0, context.getTime());
	 UT_String input2_name;
	 evalString(input2_name, "velocity_volume", 0, context.getTime());

	 //extract x y z name volume
	 UT_String vel_x;
	 UT_String vel_y;
	 UT_String vel_z;
 
	 if (input2_name.length() > 2)
	 {
	 
		 if (! ((input2_name[input2_name.length() - 1] == '*') && (input2_name[input2_name.length() - 2] == '.')))
		 {
			 std::cout << " velocity volume need a form name.* " << endl ;
			 return error();
		 }
 
		 // create name vel x y z 
		 vel_x = input2_name;
		 vel_x.removeLast();
		 vel_x.removeLast();
		 vel_y = vel_x;
		 vel_z = vel_x;

		 vel_y.append(".y");
		 vel_z.append(".z");
		 vel_x.append(".x");

		 std::cout << "name input2 : " << vel_x <<" y : "<< vel_y <<" z : "<< vel_z << endl;
	 }
 
	// loop on volumes extract handles / size
	UT_Vector3 voxel_size;
	UT_VoxelArrayWriteHandleF volume_handleW;

	GEO_Primitive* prim = nullptr;
	GA_ROHandleS attr_volume_name = GA_ROHandleS(gdp->findPrimitiveAttribute("name"));

	if (!attr_volume_name.isValid())
	{
		cout << "need name prim attribute " << endl;
		return error();
	}

	UT_String first_volume_name;
	GEO_PrimVolume* first_volume;

	//UT_Array<GEO_PrimVolume*> volumes;
	cout << "start find volumes input 1" << endl;

	int vols_found = 0;
	GA_FOR_ALL_PRIMITIVES( gdp, prim)
	{
		if (prim->getTypeId() == GA_PRIMVOLUME)		
		{
			GEO_PrimVolume* volume = (GEO_PrimVolume *)prim;
			first_volume_name = attr_volume_name.get(volume->getMapOffset());
		
			if (((first_volume_name.compare(vol_advect_name)) ==0) )
			{
				cout << "volume found  : " << first_volume_name << endl;
				first_volume = volume;
				volume_handleW = volume->getVoxelWriteHandle();
				UT_Vector3 voxel_sizeb(volume_handleW->getRes(0), volume_handleW->getRes(1), volume_handleW->getRes(2) );
				voxel_size = voxel_sizeb;

				cout << "size x " << voxel_size[0] << endl;
				cout << "size y " << voxel_size[1] << endl;
				cout << "size z " << voxel_size[2] << endl;

				vols_found++;
			}
				 
		}
	} 
 
	// loop on input 2 prims
	// find all volumes vel
	if (vols_found == 0) return error();
	

	cout << "start find volumes input 2" << endl;
	GA_ROHandleS attr_volume_nameb = GA_ROHandleS(second_input->findPrimitiveAttribute("name"));
	 
	UT_VoxelArrayWriteHandleF volume_velx_handle;
	UT_VoxelArrayWriteHandleF volume_vely_handle;
	UT_VoxelArrayWriteHandleF volume_velz_handle;
	GEO_PrimVolume* vvelx;
	GEO_PrimVolume* vvely;
	GEO_PrimVolume* vvelz;

	UT_String volume_name;
	for (GA_Iterator it(second_input->getPrimitiveRange()); (!it.atEnd() || (prim = nullptr)); ++it)
	{	
		const GEO_Primitive* prim = second_input->getGEOPrimitive(it.getOffset());

		if (prim->getTypeId() == GA_PRIMVOLUME)
		{
			GEO_PrimVolume* volume_b  = (GEO_PrimVolume *)prim;
			 volume_name = attr_volume_nameb.get(volume_b->getMapOffset());

			cout << "volume name : " << volume_name << endl;

			if (volume_name.compare(vel_x)==0)
			{
				cout << "volume found vel_x : " << volume_name << endl;
				vvelx = volume_b;
				 volume_velx_handle = volume_b->getVoxelWriteHandle();
				vols_found++;
	 		}
			if (volume_name.compare(vel_y)==0)
			{
				cout << "volume found vel_y : " << volume_name << endl;
				vvely = volume_b;
				 volume_vely_handle = volume_b->getVoxelWriteHandle();
				vols_found++;
		 	}
			 if (volume_name.compare(vel_z)==0)
			{
				cout << "volume found vel_z : " << volume_name << endl;
				vvelz = volume_b;
				 volume_velz_handle = volume_b->getVoxelWriteHandle();
				vols_found++;
		 	} 
 
		}
	 
	} 
	 cout << "num vols found : "<<vols_found <<endl;

	 
	// read density in pos - vel backward advection



 
	for (int idx_z = 0; idx_z < voxel_size[2]; ++idx_z)
	{
		for (int idx_y = 0; idx_y < voxel_size[1]; ++idx_y)
		{
			for (int idx_x = 0; idx_x < voxel_size[0]; ++idx_x)
			{
				//get pos
				UT_Vector3 pos;
				first_volume->indexToPos(idx_x, idx_y, idx_z, pos);
				
				//get voxel from pos
				for (int i = 0; i < sub; i++)
				{
					float velxv = vvelx->getValue(pos);
					float velyv = vvely->getValue(pos);
					float velzv = vvelz->getValue(pos);

					UT_Vector3 advect(velxv, velyv, velzv);

					pos += advect * amp/sub;
				}
				//get val at pos
				float val = first_volume->getValue(pos);  
 
				//set val
				volume_handleW->setValue(idx_x, idx_y , idx_z,  val );
			}
		}
	}
 
 

	 cout << "loop over all prims done" << endl;

	 return error();
}