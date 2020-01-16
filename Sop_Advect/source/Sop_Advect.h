#pragma once
#include <SOP/SOP_Node.h>
 
#include <SOP/SOP_API.h>
#include <GEO/GEO_PrimVolume.h>

namespace M_VolAdvect {
class SOP_VolAdvect : public SOP_Node
{
public:
    // node contructor for HDK
    static OP_Node *myConstructor(OP_Network*, const char *, OP_Operator *);

    // parameter array for Houdini UI
    static PRM_Template myTemplateList[];
	//void processVolumes(const GU_Detail* input_detail, const UT_Array<GEO_PrimVolume*>& volumes, fpreal t);

protected:
    // constructor, destructor
    SOP_VolAdvect(OP_Network *net, const char *name, OP_Operator *op);

    virtual ~SOP_VolAdvect();

    // labeling node inputs in Houdini UI
    virtual const char *inputLabel(unsigned idx) const;

    // main function that does geometry processing
    virtual OP_ERROR cookMySop(OP_Context &context);
 

private:
    // helper function for returning value of parameter
    int DEBUG() { return evalInt("debug", 0, 0); }

};
}

