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
//                              avtPODICAlgorithm.C                          //
// ************************************************************************* //

#include <avtPODICAlgorithm.h>
#include <TimingsManager.h>
#include <VisItStreamUtil.h>

#ifdef PARALLEL

// ****************************************************************************
// Method:  avtPODICAlgorithm::avtPODICAlgorithm
//
// Purpose: constructor
//
// Programmer:  Dave Pugmire
// Creation:    March 21, 2012
//
// ****************************************************************************


avtPODICAlgorithm::avtPODICAlgorithm(avtPICSFilter *picsFilter, int count)
    : avtParICAlgorithm(picsFilter)
{
    maxCount = count;
}

// ****************************************************************************
// Method:  avtPODICAlgorithm::~avtPODICAlgorithm
//
// Purpose: destructor
//
// Programmer:  Dave Pugmire
// Creation:    March 21, 2012
//
// ****************************************************************************

avtPODICAlgorithm::~avtPODICAlgorithm()
{
}

// ****************************************************************************
// Method:  avtPODICAlgorithm::Initialize
//
// Purpose: Initialize
//
// Programmer:  Dave Pugmire
// Creation:    March 21, 2012
//
// ****************************************************************************

void
avtPODICAlgorithm::Initialize(std::vector<avtIntegralCurve *> &seeds)
{
    int numRecvs = 64;
    if (numRecvs > nProcs)
        numRecvs = nProcs-1;

    avtParICAlgorithm::InitializeBuffers(seeds, 1, 1, numRecvs);
    AddIntegralCurves(seeds);
}

// ****************************************************************************
// Method:  avtPODICAlgorithm::AddIntegralCurves
//
// Purpose: Add ICs.
//
// Programmer:  Dave Pugmire
// Creation:    March 21, 2012
//
// ****************************************************************************

void
avtPODICAlgorithm::AddIntegralCurves(std::vector<avtIntegralCurve*> &ics)
{
    int nSeeds = ics.size();

    // If the seeds are sent to all procs check to make sure seeds on
    // domain boundaries do not get sent to mutliple processors.
    if( allSeedsSentToAllProcs )
    {
        // Sort the curves by their id so all processors are working on
        // the same curve at the same time.
        sort(ics.begin(), ics.end(), avtIntegralCurve::IDCompare);

        for (size_t i = 0; i < nSeeds; ++i)
        {
            avtIntegralCurve *ic = ics[i];

            // The seed is good if in the domain
            bool goodSeed =
              (!ic->blockList.empty() && DomainLoaded(ic->blockList.front()));

            // Check for a seed being on multiple processors. 
            int count = (int) goodSeed;
            SumIntAcrossAllProcessors( count );

            // If no processor owns the curve then the seed is outside
            // the domain. Keep the curve on rank 0 so to report the
            // result and delete it on all other processors.
            if( count == 0 )
            {
              if( PAR_Rank() == 0 )
              {
                ic->originatingRank = rank;
            
#ifdef USE_IC_STATE_TRACKING
                ic->InitTrk();
#endif
                terminatedICs.push_back(ic);
              }
              else
                delete ic;
            }
            else
            {
              // Check for the seed being on multiple processors.
              if( count > 1 )
              {
                // If the seed belongs to the processor, pass its rank.
                // The processor with the highest rank gets the seed.
                
                // Otherwise set the rank to -1 to exclude it.
                int proc = (goodSeed ? PAR_Rank() : -1);
                
                // Get the max bid from all the processors.
                int maxProc = UnifyMaximumValue( proc );
                
                // The seed is good only for the processor with the
                // highest rank.
                goodSeed = (proc == maxProc);
              }

              // If the seed is on one processor or is on the
              // processor with the max rank add it to the active
              // list, otherwise delete it.
              if( goodSeed )
              {
                ic->originatingRank = rank;
            
#ifdef USE_IC_STATE_TRACKING
                ic->InitTrk();
#endif
                activeICs.push_back(ic);
              }
              else
                delete ic;
            }
        }
    }

    // Seeds are sent just to this processor so make them active or inactive.
    else //if( !allSeedsSentToAllProcs )
    {
        // Get the ICs that are on this processor.
        for (size_t i = 0; i < nSeeds; ++i)
        {
            avtIntegralCurve *ic = ics[i];
            
            ic->originatingRank = rank;
            
#ifdef USE_IC_STATE_TRACKING
            ic->InitTrk();
#endif
            if (!ic->blockList.empty() && DomainLoaded(ic->blockList.front()))
                activeICs.push_back(ic);
            else
                inactiveICs.push_back(ic);
        }
    }

    if (DebugStream::Level1())
    {
        debug1 << "Proc " << PAR_Rank()
               << "  active ICs " << activeICs.size()
               << "  inactive ICs " << inactiveICs.size()
               << "  terminated ICs " << terminatedICs.size() << std::endl;

        std::ostringstream os;

        for (int i = 0; i < numDomains; i++)
        {
            BlockIDType d(i,0);
            if (OwnDomain(d))
            {
                os << i << " ";
            }
        }

        debug1 << "Proc " << PAR_Rank()
               << "  domains: [ " << os.str() << " ]" << std::endl;
    }
}

// ****************************************************************************
// Method:  avtPODICAlgorithm::PreRunAlgorithm
//
// Purpose:
//
// Programmer:  Dave Pugmire
// Creation:    March 21, 2012
//
// ****************************************************************************

void
avtPODICAlgorithm::PreRunAlgorithm()
{
    picsFilter->InitializeLocators();
}

// ****************************************************************************
// Method:  avtPODICAlgorithm::RunAlgorithm
//
// Purpose: Run algorithm.
//
// Programmer:  Dave Pugmire
// Creation:    March 21, 2012
//
// Modifications:
//
//   Hank Childs, Wed Mar 28 08:36:34 PDT 2012
//   Add support for terminated particle status.
//
// ****************************************************************************

void
avtPODICAlgorithm::RunAlgorithm()
{
    debug1 << "avtPODICAlgorithm::RunAlgorithm() "
           << "  active ICs " << activeICs.size()
           << "  inactive ICs " << inactiveICs.size()
           << "  terminated ICs " << terminatedICs.size() << std::endl;
    
    int timer = visitTimer->StartTimer();
    
    bool done = HandleCommunication();

    while (!done)
    {
        int cnt = 0;
        while (cnt < maxCount && !activeICs.empty())
        {
            avtIntegralCurve *ic = activeICs.front();
            activeICs.pop_front();

            do
            {
                AdvectParticle(ic);
            }
            while (ic->status.Integrateable() &&
                   !ic->blockList.empty() &&
                   DomainLoaded(ic->blockList.front()));

            // If the user termination criteria was reached so terminate the IC.
            if( ic->status.TerminationMet() )
                terminatedICs.push_back(ic);
            else if (ic->status.EncounteredSpatialBoundary())
            {
                if (!ic->blockList.empty() &&
                    DomainLoaded(ic->blockList.front()))
                    activeICs.push_back(ic);
                else
                    inactiveICs.push_back(ic);
            }
            // Some other termination criteria was reached so terminate the IC.
            else
                terminatedICs.push_back(ic);
            
            cnt++;
        }
        
        done = HandleCommunication();
    }

    TotalTime.value += visitTimer->StopTimer(timer, "Execute");
}

// ****************************************************************************
// Method:  avtPODICAlgorithm::HandleCommunication
//
// Purpose: Process communication.
//
// Programmer:  Dave Pugmire
// Creation:    March 21, 2012
//
// Modifications:
//
//   Dave Pugmire, Fri Mar  8 15:49:14 EST 2013
//   Bug fix. Ensure that the same IC isn't sent to the same rank. Also, when
//   an IC is received, set the domain from the particle point.
//
// ****************************************************************************

bool
avtPODICAlgorithm::HandleCommunication()
{
    int numICs = inactiveICs.size() + activeICs.size();
    
    //See if we're done.
    SumIntAcrossAllProcessors(numICs);
    MsgCnt.value++;

    //debug1<<"avtPODICAlgorithm::HandleCommunication() numICs= "<<numICs<<endl;
    if (numICs == 0)
        return true;

    //Tell everyone how many ICs are coming their way.
    int *icCounts = new int[nProcs], *allCounts = new int[nProcs];
    for (int i = 0; i < nProcs; i++)
        icCounts[i] = 0;
    
    std::list<avtIntegralCurve*>::iterator s;
    std::map<int, std::vector<avtIntegralCurve *> > sendICs;
    std::map<int, std::vector<avtIntegralCurve *> >::iterator it;
    std::list<avtIntegralCurve*> tmp;

    for (s = inactiveICs.begin(); s != inactiveICs.end(); s++)
    {
        // What to if the blocklist is empty ????????
        // Send it to the next processor ?????
        // Just do not send it back to the same processor.
        int domRank;

        if( (*s)->blockList.empty() )
          domRank = (PAR_Rank() + 1) % PAR_Size();
        else
          domRank = DomainToRank((*s)->blockList.front());
    
        icCounts[domRank]++;
            
        //Add to sending map.
        it = sendICs.find(domRank);
        if (it == sendICs.end())
        {
            std::vector<avtIntegralCurve *> v;
            v.push_back(*s);
            sendICs[domRank] = v;
        }
        else
            it->second.push_back(*s);
    }
    inactiveICs.clear();
    
    SumIntArrayAcrossAllProcessors(icCounts, allCounts, nProcs);
    bool anyToSend = false;
    for (int i = 0; i < nProcs && !anyToSend; i++)
        anyToSend = (allCounts[i] > 0);
    
    int incomingCnt = allCounts[rank];
    
    //Send out my ICs.
    for (it = sendICs.begin(); it != sendICs.end(); it++)
        SendICs(it->first, it->second);

    //Wait till I get all my ICs.
    while (incomingCnt > 0)
    {
        std::list<ICCommData> ics;
        std::list<ICCommData>::iterator s;

        RecvAny(NULL, &ics, NULL, true);
        for (s = ics.begin(); s != ics.end(); s++)
        {
            avtIntegralCurve *ic = (*s).ic;

            //See if I have this block.
            BlockIDType blk;
            std::list<BlockIDType> tmp;
            bool blockFound = false;

            while (!ic->blockList.empty())
            {
                blk = ic->blockList.front();
                ic->blockList.pop_front();
                if (DomainLoaded(blk))
                {
                    if (picsFilter->ICInBlock(ic, blk))
                    {
                        ic->status.ClearSpatialBoundary();
                        ic->blockList.clear();
                        ic->blockList.push_back(blk);
                        blockFound = true;
                        activeICs.push_back(ic);
                        break;
                    }
                }
                else
                    tmp.push_back(blk);
            }

            //IC Not in my blocks.  Terminate if blockList empty, or send to
            //block owner of next block in list.
            if (!blockFound)
            {
                ic->blockList = tmp;
                if (ic->blockList.empty())
                    terminatedICs.push_back(ic);
                else
                    inactiveICs.push_back(ic);
            }
        }
        
        incomingCnt -= ics.size();
        CheckPendingSendRequests();
    }
    
    CheckPendingSendRequests(); 
    delete [] icCounts;
    delete [] allCounts;
    
    return false;
}

#endif
