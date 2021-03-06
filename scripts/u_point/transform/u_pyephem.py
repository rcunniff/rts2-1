# (C) 2016, Markus Wildi, wildi.markus@bluewin.ch
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2, or (at your option)
#   any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software Foundation,
#   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
#   Or visit http://www.gnu.org/licenses/gpl.html.
#
#
__author__ = 'wildi.markus@bluewin.ch'

# Transform with pyephem

import ephem
import numpy as np

from datetime import datetime
from astropy import units as u
from astropy.coordinates import SkyCoord

class Transformation(object):
  def __init__(self, lg=None,obs=None,refraction_method=None):
    #
    self.lg=lg
    self.name='PE PyEphem'
    self.obs_astropy=obs
    self.refraction_method=refraction_method
    self.obs=ephem.Observer()
    # positive values are used for eastern longitudes
    longitude,latitude,height=self.obs_astropy.to_geodetic()
    self.obs.lon=longitude.radian
    self.obs.lat=latitude.radian
    # ToDo
    # print(type(height))
    # print(str(height))
    # 3236.9999999999477 m
    # print(height.meter)
    # AttributeError: 'Quantity' object has no 'meter' member
    self.obs.elevation=float(str(height).replace('m','').replace('meter',''))
    # do that later
    #self.obs.date

  def transform_to_hadec(self,tf=None,sky=None,mount_set_icrs=None):
    # only GCRS coordinates supported (mount_set_icrs: unused)
    tem=sky.temperature
    pre=sky.pressure
    hum=sky.humidity
    star=self.create_star(tf=tf,tem=tem,pre=pre)
    HA= self.obs.sidereal_time() - star.ra 
    ha=SkyCoord(ra=HA,dec=star.dec, unit=(u.radian,u.radian), frame='cirs',location=tf.location,obstime=tf.obstime,pressure=pre,temperature=tem,relative_humidity=hum)

    return ha

  def transform_to_altaz(self,tf=None,sky=None,mount_set_icrs=None):
    # only GCRS coordinates supported (mount_set_icrs: unused)
    # use ev. other refraction methods
    if sky is None:
      tem=pre=hum=0.
    else:
      tem=sky.temperature
      pre=sky.pressure
      hum=sky.humidity
      
    star=self.create_star(tf=tf,tem=tem,pre=pre)
    aa=SkyCoord(az=star.az,alt=star.alt, unit=(u.radian,u.radian), frame='altaz',location=tf.location,obstime=tf.obstime,pressure=pre,temperature=tem,relative_humidity=hum)    
    return aa

  def create_star(self,tf=None,tem=0.,pre=0.):
    dt=str(tf.obstime).replace('-','/')
    # http://stackoverflow.com/questions/27515575/how-do-i-get-a-pyephem-observers-meridian-in-epoch-of-date-coordinates
    # full post
    # https://oneau.wordpress.com/2010/07/04/astrometry-in-python-with-pyephem/#observer
    self.obs.date=ephem.Date(dt)
    self.obs.epoch=ephem.Date(dt)
    self.obs.pressure=pre
    self.obs.temp=tem
    star = ephem.FixedBody()
    star._ra = tf.ra.radian
    star._dec = tf.dec.radian
    # http://rhodesmill.org/pyephem/quick.html
    # body.compute(observer)
    star.compute(self.obs)
    return star
