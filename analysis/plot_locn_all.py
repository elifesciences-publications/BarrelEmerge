import os
import sys
sys.path.insert (0, './include')
import numpy as np
import matplotlib.pyplot as plt
# Import data loading code:
import BarrelData as bd
# Import my plotting code:
import plot as pt
import sebcolour

# Get target x/y hex to show trace for and the time step to show the
# map for from the arguments:
if len(sys.argv) < 2:
    print('Provide logdirname on cmd line.')
    exit(1)
logdirname = sys.argv[1].rstrip ('/')
print ('basename: {0}'.format(os.path.basename(logdirname)))
# time index
ti = -1

# Read the data
bdo = bd.BarrelData()
# Set True for inter-lines:
bdo.loadAnalysisData = True
bdo.loadDivisions = False
# If loadGuidance is True, then expt id map will be plotted:
bdo.loadGuidance = False
bdo.loadHexFlags = True
bdo.loadSimData = True
bdo.loadTimeStep = ti
bdo.load (logdirname)
bdo.computeLocalization()

# Plot a surface
import Surface as surf
sf = surf.Surface (19.2, 10.8)
sf.associate(bdo)

sf.showScalebar = False
sf.showAxes = False
sf.sb1 = [-1.3, -0.8]
sf.sb2 = [-0.3, -0.8]
sf.fs2 = 32
sf.sbtext = '1 mm'
sf.sbtpos = [-1.1, -1.1]
sf.sblw = 5
sf.sbfs = 48
sf.showNames = True
sf.svgNames = False
sf.showBoundaries = True
col = sebcolour.Colour()
sf.cmap = plt.cm.get_cmap('Greys')
sf.boundarylw = 1.0
sf.boundaryColour = col.black
sf.boundaryOuterHexColour = col.gray50

do_contours = False

for t in range(0,bdo.t_steps.size):

    # Compute max of c
    maxc = np.max (bdo.c[:,:,t], axis=0)

    # Either use the precomputed ID map:
    c_id = bdo.id_c[:,t]

    # Compute the colour map
    #colmap = np.zeros([bdo.nhex,3], dtype=float)
    #ii = 0
    #for oneid in c_id:
    #    colmap[ii] = bdo.gammaColour_byid[oneid]
    #    ii = ii + 1

    #sf.c = colmap # assign the colour map computed above

    # Colour map is bdo.locn[:,0] normalized
    #print ('bdo.locn shape is: {0}'.format(np.shape (bdo.locn)))
    # bdo.locn is nhexes by ntimes in shape
    lmap_max = np.max (bdo.locn[:,t])
    lmap_min = np.min (bdo.locn[:,t])
    lmap_range = lmap_max - lmap_min
    lmap = np.divide (bdo.locn[:,t], lmap_range)
    # Here, set z rather than the colour map, sf.c
    sf.z = np.subtract (lmap, lmap_min/lmap_range)

    sf.domcentres = bdo.domcentres[t]
    #if sf.showBoundaries == True:
    #    sf.domdivision = bdo.domdivision

    sf.plotPoly()

    # A single contour for each field
    if do_contours:
        for ii in range(0,bdo.N):
            c = bdo.c[ii,:,t]
            ccontour = 0.95*np.max(c)
            sf.addContour (c, ccontour, 'white', 1.0, ii, False);

    sf.addOuterBoundary()

    mapname = 'plots/locn_all/{0}_c_id_{1:06d}.png'.format(os.path.basename(logdirname), t)
    sf.F1.savefig (mapname, dpi=100, transparent=False)
    sf.resetFig()
