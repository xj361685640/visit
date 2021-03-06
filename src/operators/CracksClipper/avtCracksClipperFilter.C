/*****************************************************************************
*
* Copyright (c) 2000 - 2019, Lawrence Livermore National Security, LLC
* Produced at the Lawrence Livermore National Laboratory
* LLNL-CODE-442911
* All rights reserved.
*
* This file is  part of VisIt. For  details, see https://visit.llnl.gov/.  The
* full copyright notice is contained in the file COPYRIGHT located at the root
* of the VisIt distribution or at http://www.llnl.gov/visit/copyright.html.
*
* Redistribution  and  use  in  source  and  binary  forms,  with  or  without
* modification, are permitted provided that the following conditions are met:
*
*  - Redistributions of  source code must  retain the above  copyright notice,
*    this list of conditions and the disclaimer below.
*  - Redistributions in binary form must reproduce the above copyright notice,
*    this  list of  conditions  and  the  disclaimer (as noted below)  in  the
*    documentation and/or other materials provided with the distribution.
*  - Neither the name of  the LLNS/LLNL nor the names of  its contributors may
*    be used to endorse or promote products derived from this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT  HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR  IMPLIED WARRANTIES, INCLUDING,  BUT NOT  LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND  FITNESS FOR A PARTICULAR  PURPOSE
* ARE  DISCLAIMED. IN  NO EVENT  SHALL LAWRENCE  LIVERMORE NATIONAL  SECURITY,
* LLC, THE  U.S.  DEPARTMENT OF  ENERGY  OR  CONTRIBUTORS BE  LIABLE  FOR  ANY
* DIRECT,  INDIRECT,   INCIDENTAL,   SPECIAL,   EXEMPLARY,  OR   CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT  LIMITED TO, PROCUREMENT OF  SUBSTITUTE GOODS OR
* SERVICES; LOSS OF  USE, DATA, OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER
* CAUSED  AND  ON  ANY  THEORY  OF  LIABILITY,  WHETHER  IN  CONTRACT,  STRICT
* LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY  WAY
* OUT OF THE  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*
*****************************************************************************/

// ************************************************************************* //
//  File: avtCracksClipperFilter.C
// ************************************************************************* //

#include <avtCracksClipperFilter.h>

#include <avtCracksDensityFilter.h>
#include <avtRemoveCracksFilter.h>
#include <avtDataObject.h>
#include <avtSourceFromAVTDataset.h>
#include <avtVMetricVolume.h>
#include <Expression.h>
#include <ExpressionList.h>
#include <ParsingExprList.h>

#include <DebugStream.h>
#include <InvalidVariableException.h>

#include <string>
#include <vector>


// ****************************************************************************
//  Method: avtCracksClipperFilter constructor
//
//  Programmer: Kathleen Bonnell
//  Creation:   Thu Oct 13 08:17:36 PDT 2005
//
//  Modifications:
//    Kathleen Bonnell, Wed Sep 29 08:51:55 PDT 2010
//    Add calculateDensity.
//
// ****************************************************************************

avtCracksClipperFilter::avtCracksClipperFilter()
{
    calculateDensity = false;
}


// ****************************************************************************
//  Method: avtCracksClipperFilter destructor
//
//  Programmer: Kathleen Bonnell
//  Creation:   Thu Oct 13 08:17:36 PDT 2005
//
//  Modifications:
//
// ****************************************************************************

avtCracksClipperFilter::~avtCracksClipperFilter()
{
}


// ****************************************************************************
//  Method:  avtCracksClipperFilter::Create
//
//  Programmer: Kathleen Bonnell
//  Creation:   Thu Oct 13 08:17:36 PDT 2005
//
// ****************************************************************************

avtFilter *
avtCracksClipperFilter::Create()
{
    return new avtCracksClipperFilter();
}


// ****************************************************************************
//  Method:      avtCracksClipperFilter::SetAtts
//
//  Purpose:
//    Sets the state of the filter based on the attribute object.
//
//  Arguments:
//    a         The attributes to use.
//
//  Programmer: Kathleen Bonnell
//  Creation:   Thu Oct 13 08:17:36 PDT 2005
//
// ****************************************************************************

void
avtCracksClipperFilter::SetAtts(const AttributeGroup *a)
{
    atts = *(const CracksClipperAttributes*)a;
}


// ****************************************************************************
//  Method: avtCracksClipperFilter::Equivalent
//
//  Purpose:
//    Returns true if creating a new avtCracksClipperFilter with the given
//    parameters would result in an equivalent avtCracksClipperFilter.
//
//  Programmer: Kathleen Bonnell
//  Creation:   Thu Oct 13 08:17:36 PDT 2005
//
// ****************************************************************************

bool
avtCracksClipperFilter::Equivalent(const AttributeGroup *a)
{
    return (atts == *(CracksClipperAttributes*)a);
}


// ****************************************************************************
//  Method: avtCracksClipperFilter::Execute
//
//  Purpose:
//    Executes the cracks clipper filter.  This will remove the cracks,
//    calculate volumes for the new cells, and calculate density of
//    the new cells. 
//
//  Programmer: Kathleen Bonnell
//  Creation:   Thu Oct 13 08:17:36 PDT 2005
//
//  Modifications:
//    Kathleen Bonnell, Fri Oct 13 11:05:01 PDT 2006
//    RemoveCracks has been restructured, only called once now.
//
//    Kathleen Bonnell, Thu May  3 07:51:38 PDT 2007 
//    Made this a multi-filter pipeline, so that a new density can
//    be calculated.  Due to use of avtVMetricVolume, made the entire
//    method engine-specific.
//
//    Kathleen Bonnell, Wed Sep 29 08:52:39 PDT 2010
//    Use ivar 'calculateDensity' instead of atts var. Whether or not 
//    density is calculated is now based on if this operator was instantiated
//    via it's created variable, or not.
//
//    Kathleen Biagas, Tue Aug 14 14:56:21 MST 2012
//    Set var name for the density filter.
//
// ****************************************************************************

void
avtCracksClipperFilter::Execute(void)
{
#ifdef ENGINE
    //
    // Create an artificial pipeline. 
    //
    avtDataObject_p dObj = GetInput();
    avtDataset_p ds;
    CopyTo(ds, dObj);
    avtSourceFromAVTDataset termsrc(ds);
    avtDataObject_p data = termsrc.GetOutput();

    //
    // Do the work of removing cracks
    //
    avtRemoveCracksFilter removeCracks;
    removeCracks.SetAtts(&atts);
    removeCracks.SetInput(data);

    if (calculateDensity)
    {
        //
        // Calculate volume for the new cells 
        //
        avtVMetricVolume volume; 
        volume.SetOutputVariableName("ccvol");
        volume.SetInput(removeCracks.GetOutput());
        volume.UseVerdictHex(false);
 
        //
        // Calculate density
        //
        avtCracksDensityFilter density;
        density.SetInput(volume.GetOutput());
        density.SetVarName(varname);

        //
        // Force the network to execute
        //
        density.Update(GetGeneralContract());

        //
        // Copy output of the exectuion to the output of this fitler.
        //
        GetOutput()->Copy(*(density.GetOutput()));
    }
    else
    {
        //
        // Force the network to execute
        //
        removeCracks.Update(GetGeneralContract());

        //
        // Copy output of the exectuion to the output of this fitler.
        //
        GetOutput()->Copy(*(removeCracks.GetOutput()));
    }
#endif
}


// ****************************************************************************
//  Method: avtCracksClipperFilter::ModifyContract
//
//  Purpose:
//    Requests secondary variables needed by this filter, turns on Node and/or
//    Zone numbers when appropriate. 
//
//  Arguments:
//    contract     The original pipeline specification.
//
//  Returns:    The modified pipeline specification. 
//
//  Programmer: Kathleen Bonnell
//  Creation:   Thu Oct 13 08:17:36 PDT 2005
//
//  Modifications:
//    Kathleen Bonnell, Fri Oct 13 11:05:01 PDT 2006
//    Always request the secondary vars.  Added cracks_vol expression to
//    secondary var list.
//
//    Kathleen Bonnell, Wed Jan 10 13:30:40 PST 2007 
//    Ensure any existing secondary variables get added to the new DataSpec. 
//
//    Kathleen Bonnell, Mon May  7 15:48:42 PDT 2007
//    Added logic to add InMassVar as a secondary variable if necessary.
//
//    Kathleen Bonnell, Wed Sep 29 08:54:27 PDT 2010
//    Added logic to determine if this operator was instantiated via it's
//    'created' density var.
//
//    Kathleen Biagas, Tue Aug 14 14:56:21 MST 2012
//    Changed logic for cleaner code, and to ensure the proper variable name
//    is used when calculating density.
//
// ****************************************************************************

avtContract_p
avtCracksClipperFilter::ModifyContract(avtContract_p inContract)
{
    calculateDensity = false;

    if (strncmp(pipelineVariable, "operators/CracksClipper",
                strlen("operators/CracksClipper")) == 0)
    {
        calculateDensity = true;
        varname = pipelineVariable;
    }

    // Retrieve secondary variables, if any, to pass along to the 
    // newly created DataSpec
    avtDataRequest_p inDR = inContract->GetDataRequest();
    avtDataRequest_p outDR; 
    avtDataAttributes &inAtts = GetInput()->GetInfo().GetAttributes();
    if (calculateDensity)
    {
        outDR = new avtDataRequest(inDR, inAtts.GetMeshname().c_str());   
    }
    else
    {
        outDR = new avtDataRequest(inDR);
    }


    std::vector<CharStrRef> vars2 = inDR->GetSecondaryVariablesWithoutDuplicates();

    // Add any previously existing SecondaryVariables.
    for (size_t i = 0; i < vars2.size(); i++)
    {
        outDR->AddSecondaryVariable(*(vars2[i]));
    }
    // Add secondary variables necessary for CracksClipper
    outDR->AddSecondaryVariable(atts.GetCrack1Var().c_str());
    outDR->AddSecondaryVariable(atts.GetCrack2Var().c_str());
    outDR->AddSecondaryVariable(atts.GetCrack3Var().c_str());
    outDR->AddSecondaryVariable(atts.GetStrainVar().c_str());

    if (calculateDensity)
    {
        outDR->AddSecondaryVariable(atts.GetInMassVar().c_str());
    }


    ExpressionList *elist = ParsingExprList::Instance()->GetList();
    Expression *e = new Expression();

    std::string edef = std::string("volume2(<") + inAtts.GetMeshname() + 
                       std::string(">)");
    e->SetName("cracks_vol");
    e->SetDefinition(edef.c_str());
    e->SetType(Expression::ScalarMeshVar);
    elist->AddExpressions(*e);
    delete e;

    outDR->AddSecondaryVariable("cracks_vol");


    avtContract_p rv = new avtContract(inContract, outDR);

    //
    // Since this filter 'clips' the dataset, the zone and possibly
    // node numbers will be invalid, request them when needed.
    //
    if (inContract->GetDataRequest()->MayRequireZones() || 
        inContract->GetDataRequest()->MayRequireNodes())
    {
        if (inAtts.ValidActiveVariable())
        {
            if (inAtts.GetCentering() == AVT_NODECENT)
            {
                rv->GetDataRequest()->TurnNodeNumbersOn();
            }
            else if (inAtts.GetCentering() == AVT_ZONECENT)
            {
                rv->GetDataRequest()->TurnZoneNumbersOn();
            }
        }
        else 
        {
            // canot determine variable centering, so turn on both
            // node numbers and zone numbers.
            rv->GetDataRequest()->TurnNodeNumbersOn();
            rv->GetDataRequest()->TurnZoneNumbersOn();
        }
    }
    return rv; 
}


// ****************************************************************************
//  Method: avtCracksClipperFilter::UpdateDataObjectInfo
//
//  Purpose:
//    Informs the pipeline that this filter modifies zones.
//
//  Programmer: Kathleen Bonnell
//  Creation:   Thu Oct 13 08:17:36 PDT 2005
//
//  Modifications:
//    Kathleen Bonnell, Thu Mar  2 14:26:06 PST 2006 
//    Set ZonesSplit.
//
//    Brad Whitlock, Mon Apr  7 15:55:02 PDT 2014
//    Add filter metadata used in export.
//    Work partially supported by DOE Grant SC0007548.
//
// ****************************************************************************

void
avtCracksClipperFilter::UpdateDataObjectInfo()
{
    GetOutput()->GetInfo().GetValidity().InvalidateZones();
    GetOutput()->GetInfo().GetValidity().ZonesSplit();

    GetOutput()->GetInfo().GetAttributes().AddFilterMetaData("CracksClipper");
}


// ****************************************************************************
//  Method: avtCracksClipperFilter::PostExecute
//
//  Purpose:
//    Removes secondary variables from the pipeline that were requested 
//    during ModifyContract.
//
//  Programmer: Kathleen Bonnell
//  Creation:   Thu Oct 13 08:17:36 PDT 2005
//
//  Modifications:
//    Kathleen Bonnell, Mon May  7 15:48:42 PDT 2007
//    Added logic to remove InMassVar if necessary.
//
//    Kathleen Bonnell, Wed Sep 29 08:55:31 PDT 2010
//    Re-add the density variable to the pipeline if needed.
// 
//    Kathleen Biagas, Tue Aug 14 15:00:03 MST 2012
//    Move code that re-adds density variable to the pipeline to 
//    avtCracksDensityFilter, so the data range can be properly set.
// 
// ****************************************************************************

void
avtCracksClipperFilter::PostExecute()
{
    avtDataAttributes &outAtts = GetOutput()->GetInfo().GetAttributes();

    // Remove the secondary variable we requested before.
    outAtts.RemoveVariable(atts.GetCrack1Var().c_str());
    outAtts.RemoveVariable(atts.GetCrack2Var().c_str());
    outAtts.RemoveVariable(atts.GetCrack3Var().c_str());
    outAtts.RemoveVariable(atts.GetStrainVar().c_str());
}


// ****************************************************************************
//  Method: avtCracksClipperFilter::AdditionalPipelineFilters
//
//  Purpose:
//    Returns number of additional filters needed to execute this filter.
// 
//
//  Programmer: Kathleen Bonnell
//  Creation:   May 7, 2007 
//
//  Modifications:
//    Kathleen Bonnell, Wed Sep 29 08:52:39 PDT 2010
//    Use ivar 'calculateDensity' instead of atts var. 
//
// ****************************************************************************

int
avtCracksClipperFilter::AdditionalPipelineFilters(void)
{
    int rv = 1;
    if (calculateDensity)
    {
        rv += 2;
    }
    return rv;
}


