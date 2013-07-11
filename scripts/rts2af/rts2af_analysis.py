#!/usr/bin/python
# (C) 2011-2012, Markus Wildi, markus.wildi@one-arcsec.org
#
#   usage 
#   rts2af_analysis.py --help
#   
#
#   rts2af_analysis.py is called by rts2af_acquire.py during an rts2
#   initiated focus run. It is not intended for interactive use.
#   The first fits file is used as reference catalogue and its focuser 
#   position must be as close as possible to the real focus.
#   
#   rts2af_analysis.py has no connection via rts2.scriptcomm.py to rts2-centrald
#   Therefore logging must be done do a separate file.
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
# required packages:
# wget 'http://argparse.googlecode.com/files/argparse-1.1.zip

__author__ = 'markus.wildi@one-arcsec.org'

import sys
import re
import os
import rts2af 

class Analysis(rts2af.AFScript):
    """extract the catalgue of an images"""

    def run(self):

# read the SExtractor parameters
        paramsSexctractor= rts2af.SExtractorParams(paramsFileName=self.rtc.value('SEXREFERENCE_PARAM'))
        paramsSexctractor.readSExtractorParams()

        if paramsSexctractor==None:
            print 'EXIT'
            self.logger('run: exiting due to no sextractor parameters (see SEXREFERENCE_PARAM)')
            sys.exit(1)

# create the reference catalogue
        referenceFitsName = sys.stdin.readline().strip()
        self.referenceFitsName= self.env.expandToRunTimePath(referenceFitsName)

        self.logger.info('rts2af_analysis.py: pid: {0}, starting, reference file: {1}'.format(os.getpid(), self.referenceFitsName))

        hdur= rts2af.FitsHDU(env=self.env, fitsFileName=self.referenceFitsName)

        HDUs= None
        catr= None
        cats= None

        if hdur.headerProperties():
            HDUs= rts2af.FitsHDUs(minimumFocuserPositions=self.rtc.value('MINIMUM_FOCUSER_POSITIONS'), referenceHDU=hdur)
            catr= rts2af.ReferenceCatalogue(env=self.env, fitsHDU=hdur,SExtractorParams=paramsSexctractor)
            catr.runSExtractor()
            if catr.createCatalogue():

                if catr.cleanUpReference()==0:
                    print 'EXIT'
                    sys.stdout.flush()
                    self.logger.error('rts2af_analysis.py: exiting due to no objects found')
                    sys.exit(1)

                catr.writeCatalogue()
                cats= rts2af.Catalogues(env=self.env, referenceCatalogue=catr)
            else:
                print 'EXIT'
                sys.stdout.flush()
                self.logger.error('rts2af_analysis.py: exiting due to invalid reference catalogue or file not found: {0}'.format(self.referenceFitsName))
                sys.exit(1)

        else:
            print 'EXIT'
            sys.stdout.flush()

            self.logger.error('rts2af_analysis.py: exiting due to invalid hdur.headerProperties or file not found: {0}'.format(self.referenceFitsName))
            sys.exit(1)


        if catr.numberReferenceObjects() < self.rtc.value('MINIMUM_OBJECTS'):
            print 'EXIT'
            sys.stdout.flush()

            self.logger.error('rts2af_analysis.py: exiting due to too few sxObjects found: {0} of {1}'.format(catr.numberReferenceObjects(), runTimeConfig.value('MINIMUM_OBJECTS')))
            sys.exit(1)
        else:
            self.logger.info('rts2af_analysis.py: reference catalogue created with: {0} objects'.format(catr.numberReferenceObjects()))
        # is needed!
        print 'info: reference catalogue created'
        sys.stdout.flush()


# read the files sys.stdin.readline() normally rts2af_analysis.py is fed by rts2af_acquire.py
        while True:

            fits=None
            try:
                fits= sys.stdin.readline().strip()
            except:
                self.logger.info('rts2af_analysis.py: got EOF, breaking')
                break

            if fits==None:
                self.logger.info('rts2af_analysis.py: got None, breaking')
                break
            
            quit_match= re.search( r'^Q', fits)
            if not quit_match==None: 
                self.logger.info('rts2af_analysis.py: got Q, breaking')
                break

            if len(fits) < 10:
                self.logger.info('rts2af_analysis.py: got short, breaking')
                break

            hdu= rts2af.FitsHDU( env= self.env, fitsFileName=fits, referenceFitsHDU=hdur)
            if hdu.headerProperties():
                self.logger.debug('rts2af_analysis.py: append '+ hdu.fitsFileName)

                HDUs.fitsHDUsList.append(hdu)

                cat= rts2af.Catalogue(env=self.env, fitsHDU=hdu,SExtractorParams=paramsSexctractor, referenceCatalogue=catr)
                cat.runSExtractor()
                cat.createCatalogue()
                cat.cleanUp()

                # append the catalogue only if there are more than runTimeConfig.value('MATCHED_RATIO') sxObjects 
                cats.CataloguesAllFocPosList.append(cat)
                if cat.matching():
                    cats.CataloguesList.append(cat)
                    self.logger.error('rts2af_analysis.py: added catalogue at FOC_POS: {0}, file: {1}'.format(hdu.variableHeaderElements['FOC_POS'], hdu.fitsFileName))
                    self.logger.info('rts2af_analysis.py: added catalogue at FOC_POS: {0}, file: {1}'.format(hdu.variableHeaderElements['FOC_POS'], hdu.fitsFileName))
                else:
                    self.logger.error('rts2af_analysis.py: discarded catalogue at FOC_POS: {0}, file: {1}'.format(hdu.variableHeaderElements['FOC_POS'], hdu.fitsFileName))
            else:
                self.logger.error('rts2af_analysis.py: could not analyze file: {0}'.format(fits))


        if len(cats.CataloguesList)< 2: # that is the reference catalogue alone
            print 'EXIT'
            sys.stdout.flush()
                    
            self.logger.warn('rts2af_analysis.py: exiting due too few catalogues found')
            sys.exit(1)

#Bootes-2, ToDo
#        fitResult= cats.fitValues()
#        if not fitResult:
#            self.logger.error('rts2af_analysis.py: fit matching result failed, trying fit all objects')
#            fitResult= cats.fitAllValues()
# Bootes-2 ToDo, better fit without identifying objects on all images
        fitResult= cats.fitAllValues()


# fall back first matched objects, then all objects then weighted mean or extreme max/min
        if fitResult and not fitResult.error:
            print 'FOCUS: {0}, FWHM: {1}, TEMPERATURE: {2}, OBJECTS: {3} DATAPOINTS: {4} {5}'.format(fitResult.fwhmMinimumFocPos, fitResult.fwhmMinimum, fitResult.temperature, fitResult.objects, fitResult.nrDatapoints, fitResult.referenceFileName)
            self.logger.info('FOCUS: {0}, FWHM: {1}, TEMPERATURE: {2}, OBJECTS: {3} DATAPOINTS: {4} {5}'.format(fitResult.fwhmMinimumFocPos, fitResult.fwhmMinimum, fitResult.temperature, fitResult.objects, fitResult.nrDatapoints, fitResult.referenceFileName))
        else:
            self.logger.error('rts2af_analysis.py: fit result is erroneous')
            self.logger.warning('rts2af_analysis.py: no fit result, using either weighted mean or minFocPos or maxFocPos')
            (focpos, extreme)=cats.findExtreme()
            
            if focpos:
                print 'FOCUS: {0}, FWHM: {1}, TEMPERATURE: {2}, OBJECTS: {3} DATAPOINTS: {4} {5}'.format(focpos, extreme, -271.15, -1., -1., self.referenceFitsName)
                self.logger.info('FOCUS: {0}, FWHM: {1}, TEMPERATURE: {2}, OBJECTS: {3} DATAPOINTS: {4} {5}'.format(focpos, extreme, -271.15, -1., -1., self.referenceFitsName))
            else:
                self.logger.warning('rts2af_analysis.py: no focus available')
                print 'EXIT'
                sys.stdout.flush()


        self.logger.info('rts2af_analysis.py: pid: {0}, ending, reference file: {1}'.format(os.getpid(), self.referenceFitsName))

if __name__ == '__main__':
    Analysis(sys.argv[0],parser=None,mode=True).run()
