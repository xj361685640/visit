#*****************************************************************************
#
# Copyright (c) 2000 - 2019, Lawrence Livermore National Security, LLC
# Produced at the Lawrence Livermore National Laboratory
# LLNL-CODE-442911
# All rights reserved.
#
# This file is  part of VisIt. For  details, see https://visit.llnl.gov/.  The
# full copyright notice is contained in the file COPYRIGHT located at the root
# of the VisIt distribution or at http://www.llnl.gov/visit/copyright.html.
#
# Redistribution  and  use  in  source  and  binary  forms,  with  or  without
# modification, are permitted provided that the following conditions are met:
#
#  - Redistributions of  source code must  retain the above  copyright notice,
#    this list of conditions and the disclaimer below.
#  - Redistributions in binary form must reproduce the above copyright notice,
#    this  list of  conditions  and  the  disclaimer (as noted below)  in  the
#    documentation and/or other materials provided with the distribution.
#  - Neither the name of  the LLNS/LLNL nor the names of  its contributors may
#    be used to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT  HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR  IMPLIED WARRANTIES, INCLUDING,  BUT NOT  LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND  FITNESS FOR A PARTICULAR  PURPOSE
# ARE  DISCLAIMED. IN  NO EVENT  SHALL LAWRENCE  LIVERMORE NATIONAL  SECURITY,
# LLC, THE  U.S.  DEPARTMENT OF  ENERGY  OR  CONTRIBUTORS BE  LIABLE  FOR  ANY
# DIRECT,  INDIRECT,   INCIDENTAL,   SPECIAL,   EXEMPLARY,  OR   CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT  LIMITED TO, PROCUREMENT OF  SUBSTITUTE GOODS OR
# SERVICES; LOSS OF  USE, DATA, OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER
# CAUSED  AND  ON  ANY  THEORY  OF  LIABILITY,  WHETHER  IN  CONTRACT,  STRICT
# LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY  WAY
# OUT OF THE  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.
#*****************************************************************************
"""
 file: spipeline.py
 author: Cyrus Harrison <cyrush@llnl.gov>

 created: 2/4/2013
 description:
    Provides flow filters foundation for user defined script pipelines.

"""
import logging
#logging.basicConfig(level=logging.INFO)
logging.basicConfig(level=logging.ERROR)


try:
    import numpy as npy
    import vtk
    import vtk.util.numpy_support as vnp
except:
    pass

from ..core import Filter, Context, log

def info(msg):
    log.info(msg,"filters.spipeline")

def err(msg):
    log.error(msg,"filters.spipeline")


def as_vtkarray(a):
    if isinstance(a, npy.ndarray):
        return vnp.numpy_to_vtk(a)
    elif isinstance(a, rpy2.rinterface.SexpVector):
        return vnp.numpy_to_vtk(npy.asarray(a))
    else:
        return None 

def as_ndarray(a):
    if instance(a, vtk.vtkDataArray): 
        return vnp.vtk_to_numpy(a) 
    elif instance(a, rpy2.rinterface.SexpVector):
        return npy.asarray(a) 
    else:
        return None 


# TOD: Consider using context for source mesh?

class ScriptPipelineContext(Context):
    context_type = "script_pipeline"
    def init(self,source_mesh,primary_variable):
        self.mesh = source_mesh
        self.primary_var = primary_variable

class ScriptPipelineRegistrySource(Filter):
    # overrides standard RegistrySource
    filter_type    = "<registry_source>"
    input_ports    = []
    default_params = {}
    output_port    = True
    def execute(self):
        # fetch data from registry
        rkeys = self.context.registry_keys()
        #print rkeys
        if not self.name in rkeys:
            # try to dynam fetch of vtkDataArray from mesh
            if ":mesh" in  rkeys:
                var_name   = self.name[self.name.rfind(":")+1:]
                #print "fetch",var_name
                mesh  = self.context.registry_fetch(":mesh")
                varr = mesh.GetCellData().GetArray(var_name)
                if varr is None:
                    varr = mesh.GetPointData().GetArray(var_name)
                if not varr is None:
                    self.context.registry_add(self.name,varr)
        return self.context.registry_fetch(self.name)

class ScriptPipelineSink(Filter):
    # overrides standard RegistrySource
    filter_type    = "<sink>"
    input_ports    = ["in"]
    default_params = {}
    output_port    = True
    def execute(self):
        res = self.input("in")
    
        #get primary variable..
        #need to ensure that results that go back to VisIt
        #are at least correct size..
        varr = self.context.mesh.GetCellData().GetArray(self.context.primary_var)
        if varr is None:
            varr = self.context.mesh.GetPointData().GetArray(self.context.primary_var)

        if res is None:
            return self.context.mesh

        if isinstance(res,vtk.vtkDataSet):
            return res

        if isinstance(res,npy.ndarray):
            res = npy.ascontiguousarray(res)
            res = vnp.numpy_to_vtk(res)

        if not isinstance(res, vtk.vtkDataArray):
            if isinstance(res,npy.ndarray) or isinstance(res, (list,tuple)):
                np_tmp = npy.ascontiguousarray(res)
            else:
                np_tmp = npy.ascontiguousarray([res])

            #ensure 1 dimension before putting it in vtk..
            np_tmp = npy.ravel(np_tmp)
            #pad with zeros if incorrect size..
            if varr is not None and varr.GetDataSize() > len(np_tmp):
                np_tmp = npy.pad(np_tmp,(0,len(np_tmp)-var.GetDataSize()),'constant')
            res = vnp.numpy_to_vtk(np_tmp)

        #if isinstance(res,npy.ndarray):
        #    # create
        #    vdata = vtk.vtkFloatArray()
        #    vdata.SetNumberOfComponents(1)
        #    vdata.SetNumberOfTuples(res.shape[0])
        #    npo = vnp.vtk_to_numpy(vdata)
        #    npo[:] = res
        #    res = vdata
        if isinstance(res,vtk.vtkDataArray):
            res.SetName(self.context.primary_var)
            rset = self.context.mesh.NewInstance()
            rset.ShallowCopy(self.context.mesh)

            #only handles scalar data right now. TODO: add more support
            vtk_data = rset.GetCellData().GetScalars(self.context.primary_var)
            if vtk_data :
                rset.GetCellData().RemoveArray(self.context.primary_var)
                rset.GetCellData().AddArray(res)
                rset.GetCellData().SetScalars(res)
            else:
                rset.GetPointData().RemoveArray(self.context.primary_var)
                rset.GetPointData().AddArray(res)
                rset.GetPointData().SetScalars(res)
        else: #should not get here..
            rset = res
        return rset


class ScriptPipelineNDArrayFilter(Filter):
    filter_type    = "as_ndarray"
    input_ports    = ["in"]
    default_params = {}
    output_port    = True
    def execute(self):
        # handle vtkDataArray to numpy ndarray
        vdata = self.input("in")
        return vnp.vtk_to_numpy(vdata)
        
class ScriptPipelinePythonBaseFilter(Filter):
    def execute(self):
        inputs = [self.input(v) for v in self.input_ports]
        script_globals = {}
        #script_locals  = {}
        # place input names in our env
        output_stub = "def setout(v): globals()['__out_val'] = v\n"
        for v in self.input_ports:
            script_globals[v] = self.input(v)
        script_globals['as_vtkarray'] = as_vtkarray
        script_globals['as_ndarray'] = as_ndarray
        exec(output_stub,script_globals,script_globals)
        exec(self.filter_source,script_globals,script_globals)
        return script_globals["__out_val"]

def ScriptFilter(filter_name,filter_info):
    f = filter_info
    cname = "ScriptFilter" +  filter_name[0].upper() + filter_name[1:]
    cdct =  {"filter_type": filter_name,
             "input_ports": f["vars"],
             "default_params": {},
             "output_port":  True,
             "filter_source": f["source"]}
    return type(str(cname),(ScriptPipelinePythonBaseFilter,),cdct)

contexts = [ScriptPipelineContext]
filters = [ScriptPipelineRegistrySource,
           ScriptPipelineSink,
           ScriptPipelineNDArrayFilter]
def register_scripts(scripts):
    for filter_name,filter_info in scripts.items():
        filters.append(ScriptFilter(filter_name,filter_info))

