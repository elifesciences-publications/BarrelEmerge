#
# Process sensitivity analysis results gained from varying the first molecular
# signalling gradient's phi and gain.
#

import sys
import os
sys.path.insert (0, './include')
import numpy as np
import BarrelData as bd
import h5py
import math

basedir = '../logs/sensitivity_guide1/'
# Sensitivity analysis carried out for t=30000
tpoint = 30000

table = []
for logdirname in os.listdir(basedir):
    print ('Log dir: {0}'.format(logdirname))

    # Get phi1, gain1 from filename.
    spts = logdirname.split('_')
    ff_phi1 = 0
    ff_gain1 = 0
    for pt in range(0, len(spts)):
        print ('part of string: {0}'.format(spts[pt]))
        if 'p1' in spts[pt]:
            ff_phi1 = float(spts[pt+1])
        if 'g1' in spts[pt]:
            ff_gain1 = float(spts[pt+1])

    logdirname = basedir + logdirname

    # Load the dirichlet data, domcentres, etc
    bdo = bd.BarrelData()
    bdo.loadAnalaysisData = True
    bdo.loadPositions = False # required for totalarea
    bdo.loadGuidance = True # For adjacency
    bdo.loadHexFlags = True # also for adjacency
    bdo.loadSimData = True # Need sim data to do any analysis of the c values, e.g. max me - max others.
    bdo.loadDivisions = False
    bdo.debug=True
    try:
        bdo.load (logdirname)
    except:
        print ('Failed to load BarrelData object')
        exit (1)

    # Clean zeros out of honda delta and sos_distances and make a mask
    hondadelta = np.ma.masked_equal (bdo.honda, 0)
    sos_dist =  np.ma.masked_equal (bdo.sos_dist, 0)
    mask_combined = np.invert(hondadelta.mask | sos_dist.mask)
    # Apply the mask to the time:
    t1_masked = bdo.t_steps[mask_combined].T
    #print ('t1_masked: {0}'.format (t1_masked))
    t1 = t1_masked[:,0]
    #print ('t1: {0}'.format (t1))
    mapdiff = bdo.mapdiff[mask_combined].T
    area_diff = bdo.area_diff[mask_combined].T
    sos_dist = np.sqrt(sos_dist.compressed()/bdo.N)
    hondadelta = hondadelta.compressed()
    # So here, I have t, honda, sos at 1 time point, providing one table line.
    bdo.computeLocalization()
    bdo.computeAdjacencyMeasure()
    # Eta is a combined measure
    eta = area_diff[0,0] * bdo.mean_adjacency_differencemag[0] / bdo.mean_adjacency_arrangement[0]
    print ('eta: {0}'.format(eta))

    tableline = [ff_phi1, ff_gain1, hondadelta[0], sos_dist[0], area_diff[0,0], bdo.locn_vs_t[0], eta]
    table.append (tableline)

import csv
with open('postproc/sensitivity_guide1.csv', 'w', newline='\n') as csvfile:
    cw = csv.writer (csvfile, delimiter=',')
    cw.writerow (['phi1','gain1','hondadelta','sos_dist','area_diff','localization', 'eta'])
    for tableline in table:
        cw.writerow (tableline)
