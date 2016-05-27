import numpy as np
import scipy as sp
import copy

from astropy import constants as const
from astropy import units as u
from astropy.io import fits 
from astropy import log
import astropy.nddata as ndd
import numpy.ma as ma
import astropy.wcs as astrowcs
import traceback


__all__ = ["AData"]


class AData(ndd.NDData):
    """
    A generic represenation of astronomical data.
    A spectra is a 3D cube with ra_axis and dec_axis of size 1
    An image is a 3D cube with nu_axis of size 1
    A spectroscopic cube is a 3D cube
    Stokes 4D cubes are not supported.
    """
    def __init__(self,data,wcs,meta,unit):
        ndd.NDData.__init__(self,data, uncertainty=None, mask=np.isnan(data), wcs=wcs, meta=meta, unit=unit)
        self.data = ma.masked_array(data, mask=np.isnan(data))
 
    @property
    def data(self):
        return self._data
        

    @data.setter
    def data(self, value):
        self._data = value
    
    def empty_like(self):
    	dat=np.zeros_like(self.data)
    	cb=AData(dat,self.wcs,self.meta,self.unit)
    	return cb

    def copy(self):
        return copy.deepcopy(self)

    def estimate_rms(self):
       mm=self.data * self.data
       rms=np.sqrt(mm.sum()*1.0/mm.count())
       return rms


    # TODO: get_flux should be for any slice, need more parameters
    def flux(self):
    	return np.sum(self.data)   
    
    def count(self):
        return self.data.count()
    
    def shape(self):
        return self.data.shape
   
    def max(self):
        index=np.unravel_index(self.data.argmax(),self.data.shape)
        y=self.data[index]
        return (y,index)
    
    def min(self):
        index=np.unravel_index(self.data.argmin(),self.data.shape)
        y=self.data[index]
        return (y,index)

    def variance(self):
        return self.data.std()

    def stack(self,lower=None,upper=None,axis=(0)):
        sli=self._slice(lower,upper)
        return np.sum(self.data[sli],axis=axis)

    def fix_limits(self,vect):
        if isinstance(vect,tuple):
            vect=np.array(vect)
        vect=vect.astype(int)
        low=vect < 0
        up=vect > self.data.shape
        if vect.any():
            vect[low]=0
        if vect.any():
            vect[up]=np.array(self.data.shape)[up]
        return vect



    def _slice(self,lower,upper):
        #traceback.print_stack()
        if lower==None:
            lower=(0,0,0)
        if upper==None:
            upper=self.data.shape
        lower=self.fix_limits(lower)
        upper=self.fix_limits(upper)
        #if isinstance(lower,tuple):
        #   lower=np.array(lower)
        #if isinstance(upper,tuple):
        #   upper=np.array(upper)
        #lower=lower.astype(int)
        #upper=upper.astype(int)
        #llc=lower < 0
        #ulc=lower > self.data.shape
        #luc=upper < 0
        #uuc=upper > self.data.shape
        #if llc.any():
        #   log.warning("Negative lower index "+str(lower)+". Correcting to zero.")
        #   lower[llc]=0
        #if ulc.any():
        #   log.warning("Lower index out of bounds "+str(lower)+" > "+str(self.data.shape)+". Correcting to max.")
    	#   upper[ulc]=np.array(self.data.shape)[ulc]
    	#if luc.any():
    	#   log.warning("Negative upper index "+str(upper)+". Correcting to zero.")
    	#   lower[luc]=0
    	#if uuc.any():
    	#   log.warning("Upper index out of bounds "+str(upper)+" > "+str(self.data.shape)+". Correcting to max.")
    	#   upper[uuc]=np.array(self.data.shape)[uuc]
        #p#rint "fix",lower,upper
        return [slice(lower[0],upper[0]),slice(lower[1],upper[1]),slice(lower[2],upper[2])]
    			
#now it works for reals! (Axel... enjoy politics!)
#by default is uses degree 3 splines interpolation
    def scale(self, factor, order=3):
        #In case of a single float factor for all dimensions
        if type(factor)==float:
            if factor==1.:
                return self.data
            new_data = sp.ndimage.zoom(self.data, factor, order)
            return new_data
        #In case if differente scaling for each dimension    
        elif type(factor)==tuple and len(factor)==3:
            new_data = sp.ndimage.zoom(self.data, factor, order)
            return new_data
        #In other case   
        else:
            return None
        return
    
    def rotate(self, angle):
        if (angle != 0):
            new_data = self.data.rotate(angle, Image.BICUBIC,1)
            return new_data
        else:
            return self.data
    							 
    def cut(self,lower=None,upper=None):
        sli=self._slice(lower,upper)
        return self.data[sli[0],sli[1],sli[2]]

    	 
    # WCS
    
    def index_to_wcs(self,index):
        val=self.wcs.wcs_pix2world([index[::-1]],0)
        if val.shape[0]==1: val=val[0]
        return val
    
    def axis_names(self):
        return self.wcs.axis_type_names

    def wcs_limits(self,axis):
        lower=self.wcs.wcs_pix2world([[0,0,0]], 0) - self.wcs.wcs.cdelt/2.0
        shape=self.data.shape
        shape=[shape[::-1]]
        upper=self.wcs.wcs_pix2world(shape, 1) + self.wcs.wcs.cdelt/2.0
        return (lower[0][axis],upper[0][axis])    
     
    def index_features(self,lower=None,upper=None):
        sli=self._slice(lower,upper)
        x=np.arange(sli[0].start,sli[0].stop)
        y=np.arange(sli[1].start,sli[1].stop)
        z=np.arange(sli[2].start,sli[2].stop)
        xyz=np.meshgrid(x,y,z,indexing='ij')
        ii=np.empty((3,len(x)*len(y)*len(z)))
        ii[2]=xyz[0].ravel()
        ii[1]=xyz[1].ravel()
        ii[0]=xyz[2].ravel()
        return ii

    def features(self,lower=None,upper=None):
        ii=self.index_features(lower,upper)
        f=self.wcs.wcs_pix2world(ii.T,0)
        f=f.T
        return f
    
    def index_from_window(self,wcs_center,wcs_window):
        ld=np.rint(self.wcs.wcs_world2pix([wcs_center-wcs_window],0))
        lu=np.rint(self.wcs.wcs_world2pix([wcs_center+wcs_window],0))
        lower=np.array([ld,lu]).min(axis=0)
        upper=np.array([ld,lu]).max(axis=0)
        lower=self.fix_limits(lower[0][::-1])
        upper=self.fix_limits(upper[0][::-1])
        return (lower,upper)


    # Make modifications
       
    def standarize(self):
        y_min=self.data.min()
        self.data=self.data - y_min
        y_fact=self.data.sum()
        self.data=self.data/y_fact
        return (y_min,y_fact)


    def unstandarize(self, y):
        y_min = y[0]; y_fact = y_[1]
        self.data=self.data*y_fact + y_min

    
    def add_flux(self,flux,lower=None,upper=None):
        sli=self._slice(lower,upper)
        fl=np.array([0,0,0])
        fu=np.array(flux.shape)
        for i in range(0,3):
            if sli[i].start == 0:
                fl[i]=flux.shape[i] - sli[i].stop
            if sli[i].stop == self.data.shape[i]:
                fu[i]=sli[i].stop - sli[i].start
        self.data[sli]+=flux[fl[0]:fu[0],fl[1]:fu[1],fl[2]:fu[2]]


    def replace_flux(self,flux,lower=None,upper=None):
        sli=self._slice(lower,upper)
        fl=np.array([0,0,0])
        fu=np.array(flux.shape)
        for i in range(0,3):
            if sli[i].start == 0:
                fl[i]=flux.shape[i] - sli[i].stop
            if sli[i].stop == self.data.shape[i]:    		    
                fu[i]=sli[i].stop - sli[i].start
        self.data[sli]=flux[fl[0]:fu[0],fl[1]:fu[1],fl[2]:fu[2]]


    def get_hdu(self,primary=False):
        if primary==True:
            hdu = fits.PrimaryHDU(self.data.data)
        else:
            hdu = fits.ImageHdu(self.data.data)
        if self.meta is not None:
            for k, v in self.meta.iteritems():
                hdu.header[k] = v
        return hdu


    def get_mesh(self):
        sh=self.shape()
        xi, yi, zi = np.mgrid[0:sh[0], 0:sh[1], 0:sh[2]]
        return xi,yi,zi

    def get_ranges(self,lower=None,upper=None):  
        if lower==None:
            lower=[0,0,0]
        if upper==None:
            upper=self.data.shape
        lower=lower[::-1]
        lwcs=self.wcs.wcs_pix2world([lower], 0)
        lwcs=lwcs[0]
        upper=upper[::-1]
        uwcs=self.wcs.wcs_pix2world([sh], 0)
        uwcs=uwcs[0]
        lfreq=lwcs[2]*u.Hz
        ufreq=uwcs[2]*u.Hz
        rfreq=self.wcs.wcs.restfrq*u.Hz
        eq= u.doppler_radio(rfreq)
        lvel=lfreq.to(u.km/u.s, equivalencies=eq)
        uvel=ufreq.to(u.km/u.s, equivalencies=eq)
        ranges=[lvel.value,uvel.value,lwcs[1],uwcs[1],lwcs[0],uwcs[0]]      
        return ranges

