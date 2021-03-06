/*****************************************************************************
*
* Copyright (c) 2000 - 2019, Lawrence Livermore National Security, LLC
* Produced at the Lawrence Livermore National Laboratory
* All rights reserved.
*
* This file is part of VisIt. For details, see http://www.llnl.gov/visit/. The
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
*    documentation and/or materials provided with the distribution.
*  - Neither the name of the UC/LLNL nor  the names of its contributors may be
*    used to  endorse or  promote products derived from  this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT  HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR  IMPLIED WARRANTIES, INCLUDING,  BUT NOT  LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND  FITNESS FOR A PARTICULAR  PURPOSE
* ARE  DISCLAIMED.  IN  NO  EVENT  SHALL  THE  REGENTS  OF  THE  UNIVERSITY OF
* CALIFORNIA, THE U.S.  DEPARTMENT  OF  ENERGY OR CONTRIBUTORS BE  LIABLE  FOR
* ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT  LIMITED TO, PROCUREMENT OF  SUBSTITUTE GOODS OR
* SERVICES; LOSS OF  USE, DATA, OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER
* CAUSED  AND  ON  ANY  THEORY  OF  LIABILITY,  WHETHER  IN  CONTRACT,  STRICT
* LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY  WAY
* OUT OF THE  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*
*****************************************************************************/

// ************************************************************************* //
//                          avtParallelCoordinatesPlot.C                     //
// ************************************************************************* //

#include <avtParallelCoordinatesPlot.h>
#include <avtParallelCoordinatesFilter.h>


#include <ColorAttribute.h>

#include <avtColorTables.h>
#include <avtLevelsMapper.h>
#include <avtLookupTable.h>

#include <DebugStream.h>

#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>



// ****************************************************************************
//  Method: avtParallelCoordinatesPlot
//
//  Purpose: Constructor for the avtParallelCoordinatesPlot class.
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//   
// ****************************************************************************

avtParallelCoordinatesPlot::avtParallelCoordinatesPlot()
{
    levelsMapper  = new avtLevelsMapper;
    avtLUT        = new avtLookupTable;
    parAxisFilter = NULL;
    bgColor[0] = bgColor[1] = bgColor[2] = 1.0;  // white
}


// ****************************************************************************
//  Method: ~avtParallelCoordinatesPlot
//
//  Purpose: Destructor for the avtParallelCoordinatesPlot class.
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//   
// ****************************************************************************

avtParallelCoordinatesPlot::~avtParallelCoordinatesPlot()
{
    if (levelsMapper != NULL)
    {
        delete levelsMapper;
        levelsMapper = NULL;
    }

    if (parAxisFilter != NULL)
    {
        delete parAxisFilter;
        parAxisFilter = NULL;
    }

    if (avtLUT != NULL)
    {
        delete avtLUT;
        avtLUT = NULL;
    }
}


// ****************************************************************************
//  Method:  avtParallelCoordinatesPlot::Create
//
//  Purpose: Calls the constructor.
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//   
// ****************************************************************************

avtPlot*
avtParallelCoordinatesPlot::Create()
{
    return new avtParallelCoordinatesPlot;
}


// ****************************************************************************
//  Method: avtParallelCoordinatesPlot::SetAtts
//
//  Purpose: Sets attributes in the plot to specified values.
//
//  Arguments:
//      atts    The attribute values for this ParallelCoordinates plot.
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//   
// ****************************************************************************

void
avtParallelCoordinatesPlot::SetAtts(const AttributeGroup *a)
{
    needsRecalculation =
        atts.ChangesRequireRecalculation(*(const ParallelCoordinatesAttributes*)a);
    atts = *(const ParallelCoordinatesAttributes*)a;
    
    SetColors();

    behavior->SetRenderOrder(DOES_NOT_MATTER);
    behavior->SetAntialiasedRenderOrder(DOES_NOT_MATTER);
}


// ****************************************************************************
//  Method: avtParallelCoordinatesPlot::SetColors
//
//  Purpose: Sets RGB components of colors used in the plot.
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//    Jeremy Meredith, Fri Mar  7 19:00:20 EST 2008
//    Added primitive support for multiple timesteps to use multiple colors.
//
//    Jeremy Meredith, Wed Feb 25 16:37:49 EST 2009
//    Port to trunk.  Not doing time yet, but left in the capability.
//
//    Jeremy Meredith, Mon Apr 27 11:11:15 EDT 2009
//    Focus can now be drawn with colors graduated by population.
//    Removed the time case since I couldn't test any changes this would
//    have caused.
//
//    Kathleen Biagas, Thu Oct 16 09:12:03 PDT 2014
//    Send 'needsRecalculation' flag to levelsMapper when setting colors.
//
// ****************************************************************************

void
avtParallelCoordinatesPlot::SetColors()
{
    int redID, red, green, blue;
    ColorAttribute colorAtt;
    ColorAttributeList colorAttList;

    if (true) // TODO: !atts.GetDoTime()
    {
        int numColorEntries = 4 * 2 * PCP_CTX_BRIGHTNESS_LEVELS;
        unsigned char *plotColors = new unsigned char[numColorEntries];

        for (redID = 0; redID < numColorEntries; redID += 4)
        {
            float scale;
            if (redID < numColorEntries/2)
                scale = ((redID)/4.)/float(PCP_CTX_BRIGHTNESS_LEVELS);
            else
                scale = ((redID-numColorEntries/2)/4.)/float(PCP_CTX_BRIGHTNESS_LEVELS);
            int bgred   = int(bgColor[0]*255);
            int bggreen = int(bgColor[1]*255);
            int bgblue  = int(bgColor[2]*255);
            int hired, higreen, hiblue;
            if (redID < numColorEntries/2)
            {
                hired   = atts.GetContextColor().Red();
                higreen = atts.GetContextColor().Green();
                hiblue  = atts.GetContextColor().Blue();
            }
            else
            {
                hired   = atts.GetLinesColor().Red();
                higreen = atts.GetLinesColor().Green();
                hiblue  = atts.GetLinesColor().Blue();
            }
            red   = int(scale*hired   + (1.-scale)*bgred);
            green = int(scale*higreen + (1.-scale)*bggreen);
            blue  = int(scale*hiblue  + (1.-scale)*bgblue);

            colorAtt.SetRgba(red, green, blue, 255);
            colorAttList.AddColors(colorAtt);

            plotColors[redID  ] = (unsigned char)red;
            plotColors[redID+1] = (unsigned char)green;
            plotColors[redID+2] = (unsigned char)blue;
            plotColors[redID+3] = 255;
        }

        avtLUT->SetLUTColorsWithOpacity(plotColors, 2*PCP_CTX_BRIGHTNESS_LEVELS);
        levelsMapper->SetColors(colorAttList, needsRecalculation);
        delete [] plotColors;
    }
    else
    {
        // Not implemented.....
    }
}


// ****************************************************************************
//  Method: avtParallelCoordinatesPlot::GetMapper
//
//  Purpose: Gets the levels mapper as its base class (avtMapper) for the
//           plot's class (avtPlot).
//
//  Returns: The mapper for this plot.
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//   
// ****************************************************************************

avtMapperBase *
avtParallelCoordinatesPlot::GetMapper(void)
{
    return levelsMapper;
}


// ****************************************************************************
//  Method: avtParallelCoordinatesPlot::ApplyOperators
//
//  Purpose: Applies the implied operators for a ParallelCoordinates plot, namely,
//           an avtParallelCoordinatesFilter.
//
//  Arguments:
//      input   The input data object.
//
//  Returns:    The data object after the ParallelCoordinates filter is applied.
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//   
//    Hank Childs, Mon Apr  6 14:15:56 PDT 2009
//    Register named selections with the filter.
//
// ****************************************************************************

avtDataObject_p
avtParallelCoordinatesPlot::ApplyOperators(avtDataObject_p input)
{
    if (parAxisFilter != NULL)
    {
        delete parAxisFilter;
        parAxisFilter = NULL;
    }

    parAxisFilter = new avtParallelCoordinatesFilter(atts);
    for (size_t i = 0 ; i < namedSelections.size() ; i++)
    {
        parAxisFilter->RegisterNamedSelection(namedSelections[i]);
    }

    parAxisFilter->SetInput(input);

    return parAxisFilter->GetOutput();
}

// ****************************************************************************
//  Method: avtParallelCoordinatesPlot::ApplyRenderingTransformation
//
//  Purpose: Performs the rendering transformation for a ParallelCoordinates plot,
//           namely, no transformations at all.
//
//  Arguments:
//      input   The input data object.
//
//  Returns:    The input data object.
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//   
// ****************************************************************************

avtDataObject_p
avtParallelCoordinatesPlot::ApplyRenderingTransformation(avtDataObject_p input)
{
    return input;
}


// ****************************************************************************
//  Method: avtParallelCoordinatesPlot::CustomizeBehavior
//
//  Purpose: Customizes the behavior of the output.  
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//   
// ****************************************************************************

void
avtParallelCoordinatesPlot::CustomizeBehavior(void)
{
    behavior->GetInfo().GetAttributes().SetWindowMode(WINMODE_AXISARRAY);

    behavior->SetShiftFactor(0.0);
    behavior->SetLegend(NULL);
}


// ****************************************************************************
//  Method: avtParallelCoordinatesPlot::CustomizeMapper
//
//  Purpose: (Currently just a place holder).
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//   
// ****************************************************************************

void
avtParallelCoordinatesPlot::CustomizeMapper(avtDataObjectInformation &info)
{
//  May need to do something here in the future.
    return;
}


// ****************************************************************************
//  Method: avtParallelCoordinatesPlot::EnhanceSpecification
//
//  Purpose: Make sure that all axis variables are specified as secondary
//           variables except those which are components of the pipeline
//           variable.
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//    Jeremy Meredith, Thu Feb  7 17:51:12 EST 2008
//    Exit early if we had an array variable.
//
//    Jeremy Meredith, Fri Feb 15 13:16:46 EST 2008
//    Renamed orderedAxisNames to scalarAxisNames to distinguish these
//    as names of actual scalars instead of just display names.
//
//    Hank Childs, Wed Nov 17 17:19:34 PST 2010
//    Calculate the extents of secondary variables.
//
// ****************************************************************************

avtContract_p
avtParallelCoordinatesPlot::EnhanceSpecification(avtContract_p in_spec)
{
    if (atts.GetScalarAxisNames().size() == 0)
    {
        // nothing to do; this means we have an array variable
        return in_spec;
    }

    if (!atts.AttributesAreConsistent())
    {
        debug3 << "PCP/aPAP/ES/1: ParallelCoordinates plot attributes are "
               << "inconsistent." << endl;
        return in_spec;
    }

    stringVector curAxisVarNames = atts.GetScalarAxisNames();
    stringVector needSecondaryVars;
    const char *inPipelineVar = in_spec->GetDataRequest()->GetVariable();
    std::string outPipelineVar(inPipelineVar);
    std::string axisVarName;
    size_t axisNum;

    avtContract_p outSpec;

    for (axisNum = 0; axisNum < curAxisVarNames.size(); axisNum++)
    {
        if (curAxisVarNames[axisNum] == outPipelineVar) break;
    }
    
    if (axisNum < curAxisVarNames.size())
    {
        outSpec = new avtContract(in_spec);
    }
    else
    {
        outPipelineVar = curAxisVarNames[0];
        
        avtDataRequest_p newDataSpec = new avtDataRequest(
            in_spec->GetDataRequest(), outPipelineVar.c_str());
        outSpec = new avtContract(in_spec, newDataSpec);
    }

    for (axisNum = 0; axisNum < curAxisVarNames.size(); axisNum++)
    {
        if ((axisVarName = curAxisVarNames[axisNum]) != outPipelineVar)
        {
            needSecondaryVars.push_back(axisVarName);
        }
    }
        
    const std::vector<CharStrRef> curSecondaryVars =
        in_spec->GetDataRequest()->GetSecondaryVariables();
    size_t needSecVNum, curSecVNum;
    const char *needSecondaryVar;
    const char *curSecondaryVar;

    for (needSecVNum = 0; needSecVNum < needSecondaryVars.size(); needSecVNum++)
    {
        needSecondaryVar = needSecondaryVars[needSecVNum].c_str();

        for (curSecVNum = 0; curSecVNum < curSecondaryVars.size(); curSecVNum++)
        {
            if (strcmp(*curSecondaryVars[curSecVNum], needSecondaryVar) == 0)
            {
                break;
            }
        }

        if (curSecVNum >= curSecondaryVars.size())
        {
            outSpec->GetDataRequest()->AddSecondaryVariable(needSecondaryVar);
            outSpec->SetCalculateVariableExtents(needSecondaryVar, true);
        }
    }

    for (curSecVNum = 0; curSecVNum < curSecondaryVars.size(); curSecVNum++ ) {
        curSecondaryVar = *curSecondaryVars[curSecVNum];

        for (needSecVNum = 0; needSecVNum < needSecondaryVars.size(); needSecVNum++)
        {
            if (strcmp(needSecondaryVars[needSecVNum].c_str(),curSecondaryVar) == 0)
            {
                break;
            }
        }

        if (needSecVNum >= needSecondaryVars.size())
        {
            outSpec->GetDataRequest()->RemoveSecondaryVariable(curSecondaryVar);
        }
    }

    return outSpec;
}

// ****************************************************************************
//  Method: avtParallelCoordinatesPlot::ReleaseData
//
//  Purpose: Release the problem-sized data associated with this plot.
//
//  Programmer: Jeremy Meredith
//  Creation:   January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//   
// ****************************************************************************

void
avtParallelCoordinatesPlot::ReleaseData(void)
{
    avtSurfaceDataPlot::ReleaseData();

    if (parAxisFilter != NULL) parAxisFilter->ReleaseData();
}


// ****************************************************************************
//  Method: avtParallelCoordinatesPlot::SetBackgroundColor
//
//  Purpose:
//    Sets the background color.
//
//  Returns:    True if using this color will require the plot to be redrawn.
//
//  Programmer: Jeremy Meredith
//  Creation:  January 31, 2008
//
//  Note: original implementation from Mark Blair's parallel axis plot
//
//  Modifications:
//
// ****************************************************************************

bool
avtParallelCoordinatesPlot::SetBackgroundColor(const double *bg)
{
    if (bgColor[0] == bg[0] && bgColor[1] == bg[1] && bgColor[2] == bg[2])
    {
        return false;
    }

    bgColor[0] = bg[0];
    bgColor[1] = bg[1];
    bgColor[2] = bg[2];
    SetColors();

    return true;
}
