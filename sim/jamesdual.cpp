#ifdef COPYLEFT
/*
 *  This file is part of BarrelEmerge.
 *
 *  BarrelEmerge is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  BarrelEmerge is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with BarrelEmerge.  If not, see <https://www.gnu.org/licenses/>.
 */
#endif

/*
 * Based on james1.cpp, this implements dual RD systems to replicate the FGF mis-expression
 * experiment.
 *
 * Author: Seb James <seb.james@sheffield.ac.uk>
 *
 * Date: November 2019
 */

/*!
 * This will be passed as the template argument for RD_Plot and RD and
 * should be defined when compiling.
 */
#ifndef FLT
// Check CMakeLists.txt to change to double or float
# error "Please define FLT when compiling (hint: See CMakeLists.txt)"
#endif

/*!
 * General STL includes
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <string>
#include <limits>

/*!
 * Include the reaction diffusion class
 */
#include "rd_james_dncomp_dual.h"

/*!
 * And some other classes from the morph namespace
 */
#include "morph/ShapeAnalysis.h"
using morph::ShapeAnalysis;
#include "morph/Hex.h"
using morph::Hex;
#include "morph/HexGrid.h"
using morph::HexGrid;
#include "morph/HdfData.h"
using morph::HdfData;

#ifdef COMPILE_PLOTTING
/*!
 * Include display and plotting code
 */
# include "morph/display.h"
# include "morph/RD_Plot.h"
using morph::RD_Plot;
#endif

/*!
 * Included for directory manipulation code
 */
#include "morph/tools.h"

/*!
 * A class to launch a process (git status) and obtain the output.
 */
#include "morph/Process.h"
using morph::Process;
using morph::ProcessData;
using morph::ProcessCallbacks;

/*!
 * I'm using JSON to read in simulation parameters
 */
#include <json/json.h>

using namespace std;

/*!
 * Callbacks class extends ProcessCallbacks
 */
class RDProcessCallbacks : public ProcessCallbacks
{
public:
    RDProcessCallbacks (ProcessData* p) {
        this->parent = p;
    }
    void startedSignal (std::string msg) {}
    void errorSignal (int err) {
        this->parent->setErrorNum (err);
    }
    void processFinishedSignal (std::string msg) {
        this->parent->setProcessFinishedMsg (msg);
    }
    void readyReadStandardOutputSignal (void) {
        this->parent->setStdOutReady (true);
    }
    void readyReadStandardErrorSignal (void) {
        this->parent->setStdErrReady (true);
    }
private:
    ProcessData* parent;
};

/*!
 * main(): Run a simulation, using parameters obtained from a JSON
 * file.
 */
int main (int argc, char **argv)
{
    // Randomly set the RNG seed
    srand (morph::Tools::randomSeed());

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " /path/to/params.json [/path/to/logdir]" << endl;
        return 1;
    }
    string paramsfile (argv[1]);

    /*
     * Set up JSON code for reading the parameters
     */

    // Test for existence of the JSON file.
    ifstream jsonfile_test;
    int srtn = system ("pwd");
    if (srtn) {
        cerr << "system call returned " << srtn << endl;
    }
    jsonfile_test.open (paramsfile, ios::in);
    if (jsonfile_test.is_open()) {
        // Good, file exists.
        jsonfile_test.close();
    } else {
        cerr << "json config file " << paramsfile << " not found." << endl;
        return 1;
    }

    // Parse the JSON
    ifstream jsonfile (paramsfile, ifstream::binary);
    Json::Value root;
    string errs;
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    bool parsingSuccessful = Json::parseFromStream (rbuilder, jsonfile, &root, &errs);
    if (!parsingSuccessful) {
        // report to the user the failure and their locations in the document.
        cerr << "Failed to parse JSON: " << errs;
        return 1;
    }

    /*
     * Get simulation-wide parameters from JSON
     */
    const unsigned int steps = root.get ("steps", 1000).asUInt();
    if (steps == 0) {
        cerr << "Not much point simulating 0 steps! Exiting." << endl;
        return 1;
    }
    const unsigned int logevery = root.get ("logevery", 100).asUInt();
    if (logevery == 0) {
        cerr << "Can't log every 0 steps. Exiting." << endl;
        return 1;
    }
    const float hextohex_d = root.get ("hextohex_d", 0.01).asFloat();
    const float boundaryFalloffDist = root.get ("boundaryFalloffDist", 0.01).asFloat();
    const string svgpath = root.get ("svgpath", "./ellipse.svg").asString();
    const string svgpath2 = root.get ("svgpath2", "./ellipse.svg").asString();
    bool overwrite_logs = root.get ("overwrite_logs", false).asBool();
    // Do we carry out dirichlet analysis? Default to true, because it's computationally cheap.
    bool do_dirichlet_analysis = root.get ("do_dirichlet_analysis", true).asBool();
    string logpath = root.get ("logpath", "fromfilename").asString();
    string logbase = "";
    if (logpath == "fromfilename") {
        // Using json filename as logpath
        string justfile = paramsfile;
        // Remove trailing .json and leading directories
        vector<string> pth = morph::Tools::stringToVector (justfile, "/");
        justfile = pth.back();
        morph::Tools::searchReplace (".json", "", justfile);
        // Use logbase as the subdirectory into which this should go
        logbase = root.get ("logbase", "logs/").asString();
        if (logbase.back() != '/') {
            logbase += '/';
        }
        logpath = logbase + justfile;
        cout << "logpath: " << logpath << endl;
    }
    // logpath2 is based on logpath:
    string logpath2 = logpath + "_mirror";

    if (argc == 3) {
        string argpath(argv[2]);
        cerr << "Overriding the config-given logpath " << logpath << " with " << argpath << endl;
        logpath = argpath;
        if (overwrite_logs == true) {
            cerr << "WARNING: You set a command line log path.\n"
                 << "       : Note that the parameters config permits the program to OVERWRITE LOG\n"
                 << "       : FILES on each run (\"overwrite_logs\" is set to true)." << endl;
        }
    }

    // Used to initialise a
    const double aNoiseGain = root.get ("aNoiseGain", 0.1).asDouble();
    const double aInitialOffset = root.get ("aInitialOffset", 0.1).asDouble();

    const FLT dt = static_cast<FLT>(root.get ("dt", 0.00001).asDouble());

    const FLT contour_threshold = root.get ("contour_threshold", 0.6).asDouble();

    // For the maxval/contour/guidance window, used for movies
    const FLT hshift = root.get ("hshift", 0.6).asDouble();
    const FLT vshift = root.get ("vshift", 0.4).asDouble();
    const FLT g_hshift = root.get ("g_hshift", -0.2).asDouble();
    const FLT g_vshift = root.get ("g_vshift", -0.4).asDouble();

    const double D = root.get ("D", 0.1).asDouble();
    const FLT k = root.get ("k", 3).asDouble();

    const FLT l = root.get ("l", 1).asDouble();
    const FLT m = root.get ("m", 1e-8).asDouble();
    const double E = root.get ("E", 0.1).asDouble();
    cout << "E is set to " << E << endl;

    cout << "steps to simulate: " << steps << endl;

    // Thalamocortical populations array of parameters:
    const Json::Value tcs = root["tc"];
    unsigned int N_TC = static_cast<unsigned int>(tcs.size());
    if (N_TC == 0) {
        cerr << "Zero thalamocortical populations makes no sense for this simulation. Exiting."
             << endl;
        return 1;
    }

    // Guidance molecule array of parameters:
    const Json::Value guid = root["guidance"];
    unsigned int M_GUID = static_cast<unsigned int>(guid.size());

#ifdef COMPILE_PLOTTING

    // Parameters from the config that apply only to plotting:
    const unsigned int plotevery = root.get ("plotevery", 10).asUInt();

    // If true, then write out the logs in consecutive order numbers,
    // rather than numbers that relate to the simulation timestep.
    const bool vidframes = root.get ("vidframes", false).asBool();
    unsigned int framecount = 0;

    // Which windows to plot?
    const bool plot_guide = root.get ("plot_guide", true).asBool();
    const bool plot_contours = root.get ("plot_contours", true).asBool();
    const bool plot_a_contours = root.get ("plot_a_contours", true).asBool();
    const bool plot_a = root.get ("plot_a", true).asBool();
    const bool scale_a = root.get ("scale_a", true).asBool();
    const bool plot_c = root.get ("plot_c", true).asBool();
    const bool scale_c = root.get ("scale_c", true).asBool();
    const bool plot_n = root.get ("plot_n", true).asBool();
    const bool plot_dr = root.get ("plot_dr", true).asBool();
    // Should the guidance be plotted in same window as the dr stuff?
    const bool plot_dr_with_guide = root.get ("plot_dr_with_guide", false).asBool();
    const bool scale_n = root.get ("scale_n", true).asBool();
    // Window IDs
    unsigned int guide_id = 0xffff, contours_id = 0xffff, a_id = 0xffff, c_id = 0xffff, n_id = 0xffff, dr_id = 0xffff, dr2_id = 0xffff, a_contours_id = 0xffff;

    const bool plot_guidegrad = root.get ("plot_guidegrad", false).asBool();
    const bool plot_divg = root.get ("plot_divg", false).asBool();
    const bool plot_divJ = root.get ("plot_divJ", false).asBool();
    unsigned int guidegrad_x_id = 0xffff;
    unsigned int guidegrad_y_id = 0xffff;
    unsigned int divg_id = 0xffff;
    unsigned int divJ_id = 0xffff;

    // Create some displays
    vector<morph::Gdisplay> displays;
    vector<double> fix(3, 0.0);
    vector<double> eye(3, 0.0);
    eye[2] = 0.12; // This also acts as a zoom. more +ve to zoom out, more -ve to zoom in.
    vector<double> rot(3, 0.0);

    // A plot object.
    RD_Plot<FLT> plt(fix, eye, rot);
    plt.scalarFieldsSingleColour = true;

    double rhoInit = root.get ("rhoInit", 1.0).asDouble(); // This is effectively a zoom control. Increase to zoom out.
    double thetaInit = 0.0;
    double phiInit = 0.0;

    string worldName("j");
    unsigned int windowId = 0;
    string winTitle = "";

    const unsigned int win_width = root.get ("win_width", 340).asUInt();
    unsigned int win_height = static_cast<unsigned int>(0.8824f * (float)win_width);

    // Default the contours to 720p format 16:9 ratio for nice movies
    const unsigned int win_width_contours = root.get ("win_width_contours", 1280).asUInt();
    unsigned int win_height_contours = static_cast<unsigned int>(0.5625f * (float)win_width_contours);

    // SW - Contours. Always plot
    if (plot_contours) {
        winTitle = worldName + ": contours (from c)"; //3

        displays.push_back (morph::Gdisplay (win_width_contours, win_height_contours, 100, 1500,
                                             winTitle.c_str(), rhoInit*0.7, thetaInit, phiInit,
                                             (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        contours_id = windowId++;
    }
    // a contours.
    if (plot_a_contours) {
        winTitle = worldName + ": contours (from a)"; //3
        displays.push_back (morph::Gdisplay (win_width_contours, win_height_contours, 100, 1500,
                                             winTitle.c_str(), rhoInit*0.7, thetaInit, phiInit,
                                             (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        a_contours_id = windowId++;
    }

    if (plot_guide) {
        winTitle = worldName + ": Guidance molecules"; // 0
        displays.push_back (morph::Gdisplay (win_width * (M_GUID>0?M_GUID:1), win_height, 100, 300,
                                             winTitle.c_str(), rhoInit, thetaInit, phiInit,
                                             (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        guide_id = windowId++;
    }

    if (plot_a) {
        winTitle = worldName + ": a[0] to a[N]"; // 1
        displays.push_back (morph::Gdisplay (win_width*N_TC, win_height, 100, 900, winTitle.c_str(),
                                             rhoInit, thetaInit, phiInit, (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        a_id = windowId++;
    }

    if (plot_c) {
        winTitle = worldName + ": c[0] to c[N]"; // 2
        displays.push_back (morph::Gdisplay (win_width*N_TC, win_height, 100, 1200, winTitle.c_str(),
                                             rhoInit, thetaInit, phiInit, (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        c_id = windowId++;
    }

    if (plot_n) {
        winTitle = worldName + ": n"; //4
        displays.push_back (morph::Gdisplay (win_width, win_height, 100, 1800, winTitle.c_str(),
                                             rhoInit, thetaInit, phiInit, (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        n_id = windowId++;
    }

    if (plot_dr && do_dirichlet_analysis) {
        winTitle = worldName + ": dr"; //4
        displays.push_back (morph::Gdisplay (win_width_contours, win_height_contours, 100, 1800, winTitle.c_str(),
                                             rhoInit*1.2, thetaInit, phiInit, (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        dr_id = windowId++;
        winTitle = worldName + ": dr2";
        displays.push_back (morph::Gdisplay (win_width_contours, win_height_contours, 100, 1800, winTitle.c_str(),
                                             rhoInit*1.2, thetaInit, phiInit, (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        dr2_id = windowId++;
    } else if (plot_dr && !do_dirichlet_analysis) {
        cout << "Note: To plot the dirichlet regions (dr), do_dirichlet_analysis must be set." << endl;
    }

    if (plot_guidegrad) {
        winTitle = worldName + ": Guidance gradient (x)";//5
        displays.push_back (morph::Gdisplay (win_width*N_TC, win_height, 100, 1800, winTitle.c_str(),
                                             rhoInit, thetaInit, phiInit, (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        guidegrad_x_id = windowId++;

        winTitle = worldName + ": Guidance gradient (y)";//6
        displays.push_back (morph::Gdisplay (win_width*N_TC, win_height, 100, 1800, winTitle.c_str(),
                                             rhoInit, thetaInit, phiInit, (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        guidegrad_x_id = windowId++;
    }

    if (plot_divg) {
        winTitle = worldName + ": div(g)/3d";//7
        displays.push_back (morph::Gdisplay (win_width*N_TC, win_height, 100, 1800, winTitle.c_str(),
                                             rhoInit, thetaInit, phiInit, (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        divg_id = windowId++;
    }

    if (plot_divJ) {
        winTitle = worldName + ": div(J)";//8 or 5
        displays.push_back (morph::Gdisplay (win_width*N_TC, win_height, 100, 1800, winTitle.c_str(),
                                             rhoInit, thetaInit, phiInit, (windowId==0?0:displays[0].win)));
        displays.back().resetDisplay (fix, eye, rot);
        displays.back().redrawDisplay();
        divJ_id = windowId++;
    }
#endif

    /*
     * Instantiate and set up the model object and its mirror:
     */
    RD_James_dncomp_dual<FLT> RD;
    RD_James_dncomp_dual<FLT> RD2; // mirror

    RD.svgpath = svgpath;
    RD2.svgpath = svgpath2; // mirror image region

    RD.logpath = logpath;
    RD2.logpath = logpath2;

    // NB: Set .N, .M BEFORE RD.allocate().
    RD.N = N_TC; // Number of TC populations
    RD2.N = N_TC;

    RD.M = M_GUID; // Number of guidance molecules that are sculpted
    RD2.M = M_GUID;

    // Set up timestep
    RD.set_dt (dt);
    RD2.set_dt (dt);

    // Control the size of the hexes, and therefore the number of hexes in the grid
    RD.hextohex_d = hextohex_d;
    RD2.hextohex_d = hextohex_d;

    // Boundary fall-off distance
    RD.boundaryFalloffDist = boundaryFalloffDist;
    RD2.boundaryFalloffDist = boundaryFalloffDist;

    RD.aNoiseGain = aNoiseGain;
    RD2.aNoiseGain = aNoiseGain;
    RD.aInitialOffset = aInitialOffset;
    RD2.aInitialOffset = aInitialOffset;

    // After setting N and M, we can set up all the vectors in RD:
    RD.allocate();
    RD2.allocate();

    // Make the link between RD and RD2
    RD.mirror = &RD2;

    // After allocate(), we can set up parameters:
    RD.set_D (D);
    RD2.set_D (D);

    cout << "Setting RD.l to " << l << endl;
    RD.l = l;
    RD2.l = l;
    RD.m = m;
    RD2.m = m;
#ifndef E_A_DIVN
    if (E > 0.0) {
        cout << "ERROR: You have E>0.0, but you are using a binary without the code to compute E div n" << endl;
        exit (1);
    }
#endif
    RD.E = E;
    RD2.E = E;

    RD.contour_threshold = contour_threshold;
    RD2.contour_threshold = contour_threshold;
    RD.k = k;
    RD2.k = k;

    // Index through thalamocortical fields, setting params:
    for (unsigned int i = 0; i < tcs.size(); ++i) {
        Json::Value v = tcs[i];
        RD.alpha[i] = v.get("alpha", 0.0).asDouble();
        RD2.alpha[i] = v.get("alpha", 0.0).asDouble();
        RD.beta[i] = v.get("beta", 0.0).asDouble();
        RD2.beta[i] = v.get("beta", 0.0).asDouble();

        // Sets up mask for initial branching density
        GaussParams<FLT> gp;
        gp.gain = v.get("gaininit", 1.0).asDouble();
        gp.sigma = v.get("sigmainit", 0.0).asDouble();
        gp.x = v.get("xinit", 0.0).asDouble();
        cout << "Set xinit["<<i<<"] to " << gp.x << endl;
        gp.y = v.get("yinit", 0.0).asDouble();

        RD.initmasks.push_back (gp);
        // Invert x for RD2:
        gp.x = -1.0 * gp.x;
        RD2.initmasks.push_back (gp);

        RD.epsilon[i] = v.get("epsilon", 0.0).asDouble();
        RD2.epsilon[i] = v.get("epsilon", 0.0).asDouble();
        cout << "Set RD.epsilon["<<i<<"] to " << RD.epsilon[i] << endl;
    }

    // Index through guidance molecule parameters:
    for (unsigned int j = 0; j < guid.size(); ++j) {
        Json::Value v = guid[j];
        // What guidance molecule method will we use?
        string rmeth = v.get ("shape", "Sigmoid1D").asString();
        DBG ("guidance modelecule shape: " << rmeth);
        if (rmeth == "Sigmoid1D") {
            RD.rhoMethod[j] = FieldShape::Sigmoid1D;
        } else if (rmeth == "Linear1D") {
            RD.rhoMethod[j] = FieldShape::Linear1D;
        } else if (rmeth == "Exponential1D") {
            RD.rhoMethod[j] = FieldShape::Exponential1D;
        } else if (rmeth == "Gauss1D") {
            RD.rhoMethod[j] = FieldShape::Gauss1D;
        } else if (rmeth == "Gauss2D") {
            RD.rhoMethod[j] = FieldShape::Gauss2D;
        } else if (rmeth == "CircLinear2D") {
            RD.rhoMethod[j] = FieldShape::CircLinear2D;
        }
        RD2.rhoMethod[j] = RD.rhoMethod[j];

        // Set up guidance molecule method parameters
        RD.guidance_gain.push_back (v.get("gain", 1.0).asDouble());
        DBG ("guidance modelecule gain: " << RD.guidance_gain.back());
        RD.guidance_phi.push_back (v.get("phi", 1.0).asDouble());
        RD.guidance_width.push_back (v.get("width", 1.0).asDouble());
        RD.guidance_offset.push_back (v.get("offset", 1.0).asDouble());
        RD.guidance_time_onset.push_back (v.get("time_onset", 0).asUInt());

        RD2.guidance_gain.push_back (v.get("gain", 1.0).asDouble());
        // Here's the mirroring: Add 180 degrees:
        RD2.guidance_phi.push_back (180.0 + v.get("phi", 1.0).asDouble());
        RD2.guidance_width.push_back (v.get("width", 1.0).asDouble());
        RD2.guidance_offset.push_back (v.get("offset", 1.0).asDouble());
        RD2.guidance_time_onset.push_back (v.get("time_onset", 0).asUInt());
    }

    // Which of the gammas is the "group" defining gamma?
    const unsigned int groupgamma = root.get ("groupgamma", 0).asUInt();

    // Set up the interaction parameters between the different TC
    // populations and the guidance molecules (aka gamma).
    int paramRtn = 0;
    for (unsigned int i = 0; i < tcs.size(); ++i) {
        Json::Value tcv = tcs[i];
        Json::Value gamma = tcv["gamma"];
        for (unsigned int j = 0; j < guid.size(); ++j) {
            // Set up gamma values using a setter which checks we
            // don't set a value that's off the end of the gamma
            // container.
            cout << "Set gamma for guidance " << j << " over TC " << i << " = " << gamma[j] << endl;
            paramRtn += RD.setGamma (j, i, gamma[j].asDouble(), groupgamma);
            paramRtn += RD2.setGamma (j, i, gamma[j].asDouble(), groupgamma);
        }
    }

    if (paramRtn && M_GUID>0) {
        cerr << "Something went wrong setting gamma values" << endl;
        return paramRtn;
    }

    // Now have the guidance molecule densities and their gradients computed, call init()
    RD.init();
    // Reseed RNG
    srand (morph::Tools::randomSeed());
    // Init RD2
    RD2.init();

    RD.init_sums();

    /*
     * Now create a log directory if necessary, and exit on any
     * failures.
     */
    if (morph::Tools::dirExists (logpath) == false) {
        morph::Tools::createDir (logpath);
        if (morph::Tools::dirExists (logpath) == false) {
            cerr << "Failed to create the logpath directory "
                 << logpath << " which does not exist."<< endl;
            return 1;
        }
    } else {
        // Directory DOES exist. See if it contains a previous run and
        // exit without overwriting to avoid confusion.
        if (overwrite_logs == false
            && (morph::Tools::fileExists (logpath + "/params.json") == true
                || morph::Tools::fileExists (logpath + "/guidance.h5") == true
                || morph::Tools::fileExists (logpath + "/positions.h5") == true)) {
            cerr << "Seems like a previous simulation was logged in " << logpath << ".\n"
                 << "Please clean it out manually, choose another directory or set\n"
                 << "overwrite_logs to true in your parameters config JSON file." << endl;
            return 1;
        }
    }
    // Duplicate test for logpath2:
    if (morph::Tools::dirExists (logpath2) == false) {
        morph::Tools::createDir (logpath2);
        if (morph::Tools::dirExists (logpath2) == false) {
            cerr << "Failed to create the logpath directory "
                 << logpath2 << " which does not exist."<< endl;
            return 1;
        }
    } else {
        // Directory DOES exist. See if it contains a previous run and
        // exit without overwriting to avoid confusion.
        if (overwrite_logs == false
            && (morph::Tools::fileExists (logpath2 + "/params.json") == true
                || morph::Tools::fileExists (logpath2 + "/guidance.h5") == true
                || morph::Tools::fileExists (logpath2 + "/positions.h5") == true)) {
            cerr << "Seems like a previous simulation was logged in " << logpath2 << ".\n"
                 << "Please clean it out manually, choose another directory or set\n"
                 << "overwrite_logs to true in your parameters config JSON file." << endl;
            return 1;
        }
    }

    // As RD.allocate() as been called (and log directory has been
    // created/verified ready), positions can be saved to file.
    RD.savePositions();
    RD2.savePositions();
    RD.saveHG();
    //RD2.saveHG(); // If necessary

    // Save the guidance molecules now.
    RD.saveGuidance();
    RD2.saveGuidance();

#ifdef COMPILE_PLOTTING

    vector<vector<FLT> > gx = plt.separateVectorField (RD.g[0], 0);
    vector<vector<FLT> > gy = plt.separateVectorField (RD.g[0], 1);
    FLT ming = 1e7;
    FLT maxg = -1e7;
    if (plot_guide) {
        // Plot gradients of the guidance effect g.
        plt.scalarfields (displays[guide_id], RD.hg, RD.rho);
        displays[guide_id].redrawDisplay();
    }
    if (plot_guidegrad) {
        // Determine scale of gx and gy so that a common scale can be
        // applied to both gradient_x and gradient_y.
        for (unsigned int hi=0; hi<RD.nhex; ++hi) {
            Hex* h = RD.hg->vhexen[hi];
            if (h->onBoundary() == false) {
                for (unsigned int i = 0; i<RD.N; ++i) {
                    if (gx[i][h->vi]>maxg) { maxg = gx[i][h->vi]; }
                    if (gx[i][h->vi]<ming) { ming = gx[i][h->vi]; }
                    if (gy[i][h->vi]>maxg) { maxg = gy[i][h->vi]; }
                    if (gy[i][h->vi]<ming) { ming = gy[i][h->vi]; }
                }
            }
        }
        cout << "min g = " << ming << " and max g = " << maxg << endl;
    }

    FLT mindivg = 1e7;
    FLT maxdivg = -1e7;
    if (plot_divg) {
        for (unsigned int hi=0; hi<RD.nhex; ++hi) {
            Hex* h = RD.hg->vhexen[hi];
            if (h->onBoundary() == false) {
                for (unsigned int i = 0; i<RD.N; ++i) {
                    // FIXME only dealing with divg_over3d[0] here!
                    if (RD.divg_over3d[0][i][h->vi]>maxdivg) { maxdivg = RD.divg_over3d[0][i][h->vi]; }
                    if (RD.divg_over3d[0][i][h->vi]<mindivg) { mindivg = RD.divg_over3d[0][i][h->vi]; }
                }
            }
        }
        cout << "min div(g) = " << mindivg << " and max div(g) = " << maxdivg << endl;
    }

    // Now plot fields and redraw display
    if (plot_guidegrad) {
        plt.scalarfields (displays[guidegrad_x_id], RD.hg, gx, ming, maxg);
        displays[guidegrad_x_id].redrawDisplay();
        plt.scalarfields (displays[guidegrad_y_id], RD.hg, gy, ming, maxg);
        displays[guidegrad_y_id].redrawDisplay();
    }
    if (plot_divg) {
        // FIXME
        plt.scalarfields (displays[divg_id], RD.hg, RD.divg_over3d[0], mindivg, maxdivg);
        displays[divg_id].redrawDisplay();
    }
    if (plot_divJ) {
        plt.scalarfields (displays[divJ_id], RD.hg, RD.divJ);
        displays[divJ_id].redrawDisplay();
    }
    // Save images in log folder
    if (RD.M > 0 && plot_guide) { plt.savePngs (logpath, "guidance", 0, displays[guide_id]); }


    // At step 0, there's no connection/contour information to show,
    // but we can save the initial branching.
    if (plot_a) {
        if (scale_a) {
            plt.scalarfields (displays[a_id], RD.hg, RD.a); // scale between min and max
        } else {
            plt.scalarfields (displays[a_id], RD.hg, RD.a, 0.0); // scale between 0 and max
        }
        plt.savePngs (logpath, "axonbranch", 0, displays[a_id]);
    }

#endif // COMPILE_PLOTTING

    // Start the loop
    bool finished = false;
    while (finished == false) {
        // Step the model. Note that because RD has a pointer to RD2, we only have one .step() call
        RD.step();

#ifdef COMPILE_PLOTTING
        if ((RD.stepCount % plotevery) == 0) {
            DBG2("Plot at step " << RD.stepCount);
            // Do a plot of the ctrs as found.
            vector<list<Hex> > ctrs = ShapeAnalysis<FLT>::get_contours (RD.hg, RD.c, RD.contour_threshold);
            vector<list<Hex> > ctrs2 = ShapeAnalysis<FLT>::get_contours (RD2.hg, RD2.c, RD2.contour_threshold);

            if (do_dirichlet_analysis == true) {
                RD.dirichlet();
                RD2.dirichlet();
                //cout << "dirich_value = " << RD.honda << endl;
            }

            vector<list<Hex> > a_ctrs;
            if (plot_contours) {
                plt.plot_contour (displays[contours_id], RD.hg, ctrs);
            }
            if (plot_a_contours) {
                a_ctrs = ShapeAnalysis<FLT>::get_contours (RD.hg, RD.a, RD.contour_threshold);
                plt.plot_contour (displays[a_contours_id], RD.hg, a_ctrs);
            }
            if (plot_a) {
                if (scale_a) {
                    plt.scalarfields (displays[a_id], RD.hg, RD.a); // scale between min and max
                } else {
                    plt.scalarfields (displays[a_id], RD.hg, RD.a, 0.0); // scale between 0 and max
                }
            }
            if (plot_c) {
                if (scale_c) {
                    plt.scalarfields (displays[c_id], RD.hg, RD.c);
                } else {
                    plt.scalarfields (displays[c_id], RD.hg, RD.c, 0.0, 1.0);
                }
            }
            if (plot_n) {
                if (scale_n) {
                    plt.scalarfields (displays[n_id], RD.hg, RD.n);
                } else {
                    plt.scalarfields (displays[n_id], RD.hg, RD.n, 0.0, 1.0);
                }
            }
            if (plot_dr && do_dirichlet_analysis) {
                plt.scalarFieldsSingleColour = false;
                if (plot_contours) {
                    if (plot_dr_with_guide) {
                        vector<bool> on;
                        for (auto t_on : RD.guidance_time_onset) {
                            if (RD.stepCount >= t_on) {
                                on.push_back (true);
                            } else {
                                on.push_back (false);
                            }
                        }
                        plt.plot_contour_and_scalar_and_guide (displays[dr_id], RD.hg, ctrs, RD.regions, RD.rho, on,
                                                               hshift, vshift, g_hshift, g_vshift);
                    } else {
                        plt.plot_contour_and_scalar (displays[dr_id], RD.hg, ctrs, RD.regions, hshift, g_hshift);
                        plt.plot_contour_and_scalar (displays[dr2_id], RD2.hg, ctrs2, RD2.regions, hshift, g_hshift);
                    }
                } else {
                    plt.scalarfields (displays[dr_id], RD.hg, RD.regions);
                }
                plt.scalarFieldsSingleColour = true;
            }
            // Then add:
            //plt.plot_dirichlet_boundaries (displays[n_id], RD.hg, vv);

            if (plot_guidegrad) {
                displays[guidegrad_x_id].redrawDisplay();
                displays[guidegrad_y_id].redrawDisplay();
            }
            if (plot_divg) {
                displays[divg_id].redrawDisplay();
            }
            if (plot_divJ) {
                plt.scalarfields (displays[divJ_id], RD.hg, RD.divJ);
            }

            if (vidframes) {
                if (plot_c) {
                    plt.savePngs (logpath, "connections", framecount, displays[c_id]);
                }
                if (plot_a) {
                    plt.savePngs (logpath, "axonbranch", framecount, displays[a_id]);
                }
                if (plot_contours) {
                    plt.savePngs (logpath, "contours", framecount, displays[contours_id]);
                }
                if (plot_a_contours) {
                    plt.savePngs (logpath, "a_contours", framecount, displays[a_contours_id]);
                }
                if (plot_dr && do_dirichlet_analysis) {
                    plt.savePngs (logpath, "maxval", framecount, displays[dr_id]);
                }
                ++framecount;
            } else {
                if (plot_c) {
                    plt.savePngs (logpath, "connections", RD.stepCount, displays[c_id]);
                }
                if (plot_a) {
                    plt.savePngs (logpath, "axonbranch", RD.stepCount, displays[a_id]);
                }
                if (plot_contours) {
                    plt.savePngs (logpath, "contours", RD.stepCount, displays[contours_id]);
                }
                if (plot_a_contours) {
                    plt.savePngs (logpath, "a_contours", RD.stepCount, displays[a_contours_id]);
                }
                if (plot_dr && do_dirichlet_analysis) {
                    plt.savePngs (logpath, "maxval", RD.stepCount, displays[dr_id]);
                }
            }
        }
#endif // COMPILE_PLOTTING

        // Save data every 'logevery' steps
        if ((RD.stepCount % logevery) == 0) {
            cout << "Logging data at step " << RD.stepCount << endl;
            RD.save();
            RD2.save();
            if (do_dirichlet_analysis == true) {
#ifndef COMPILE_PLOTTING // Then it won't have been done above:
                RD.dirichlet();
                RD2.dirichlet();
#endif
                RD.saveDirichletDomains();
                RD2.saveDirichletDomains();
            }
        }

        if (RD.stepCount > steps) {
            finished = true;
        }
    }

    // Save out the sums.
    RD.savesums();
    RD2.savesums();

    // Before saving the json, we'll place any additional useful info
    // in there, such as the FLT. If float_width is 4, then
    // results were computed with single precision, if 8, then double
    // precision was used. Also save various parameters from the RD system.
    root["float_width"] = (unsigned int)sizeof(FLT);
    string tnow = morph::Tools::timeNow();
    root["sim_ran_at_time"] = tnow.substr(0,tnow.size()-1);
    root["hextohex_d"] = RD.hextohex_d;
    root["D"] = RD.get_D();
    root["k"] = RD.k;
    root["dt"] = RD.get_dt();
    // Call our function to place git information into root.
    morph::Tools::insertGitInfo (root, "sim/");
    // Store the binary name and command argument into root, too.
    if (argc > 0) { root["argv0"] = argv[0]; }
    if (argc > 1) { root["argv1"] = argv[1]; }

    // We'll save a copy of the parameters for the simulation in the log directory as params.json
    const string paramsCopy = logpath + "/params.json";
    ofstream paramsConf;
    paramsConf.open (paramsCopy.c_str(), ios::out|ios::trunc);
    if (paramsConf.is_open()) {
        paramsConf << root;
        paramsConf.close();
    } else {
        cerr << "Warning: Failed to open file to write a copy of the params.json" << endl;
    }

    // FIXME: Haven't duplicated this for RD2
    // Extract contours
    vector<list<Hex> > ctrs = ShapeAnalysis<FLT>::get_contours (RD.hg, RD.c, RD.contour_threshold);
    {
        // Write each contour to a contours.h5 file
        stringstream ctrname;
        ctrname << logpath << "/contours.h5";
        HdfData ctrdata(ctrname.str());
        unsigned int nctrs = ctrs.size();
        ctrdata.add_val ("/num_contours", nctrs);
        for (unsigned int ci = 0; ci < nctrs; ++ci) {
            vector<FLT> vx, vy;
            auto hi = ctrs[ci].begin();
            while (hi != ctrs[ci].end()) {
                vx.push_back (hi->x);
                vy.push_back (hi->y);
                ++hi;
            }
            stringstream ciss;
            ciss << ci;
            string pth = "/x" + ciss.str();
            ctrdata.add_contained_vals (pth.c_str(), vx);
            pth[1] = 'y';
            ctrdata.add_contained_vals (pth.c_str(), vy);

            // Generate hex grids from contours to obtain the size of the region enclosed by the contour
            HexGrid* hg1 = new HexGrid (RD.hextohex_d, 3, 0, morph::HexDomainShape::Boundary);
            hg1->setBoundary (ctrs[ci]);
            pth[1] = 'n';
            ctrdata.add_val(pth.c_str(), hg1->num());
            delete hg1;
        }

        // Also extract the boundary of the main, enclosing hexgrid and write that.
        list<Hex> outerBoundary = RD.hg->getBoundary();
        vector<FLT> vx, vy;
        auto bi = outerBoundary.begin();
        while (bi != outerBoundary.end()) {
            vx.push_back (bi->x);
            vy.push_back (bi->y);
            ++bi;
        }
        ctrdata.add_contained_vals ("/xb", vx);
        ctrdata.add_contained_vals ("/yb", vy);
    }

#ifdef COMPILE_PLOTTING
    // Ask for a keypress before exiting so that the final images can be studied
    int a;
    cout << "Press any key[return] to exit.\n";
    cin >> a;
#endif // COMPILE_PLOTTING

    return 0;
};
