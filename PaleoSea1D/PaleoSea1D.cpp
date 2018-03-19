#include "stdifm.h"
#include "PaleoSea1D.h"

IfmModule g_pMod;  /* Global handle related to this plugin */

#pragma region IFM_Definitions
/* --- IFMREG_BEGIN --- */
/*  -- Do not edit! --  */

static IfmResult OnBeginDocument (IfmDocument);
static void OnEndDocument (IfmDocument);
static void PreSimulation (IfmDocument);
static void PostSimulation (IfmDocument);
static void PreTimeStep (IfmDocument);
static void PostTimeStep (IfmDocument);

/*
 * Enter a short description between the quotation marks in the following lines:
 */
static const char szDesc[] = 
  "PaleoSea1D is an IFM plugin for FEFLOW for simulating diffusive or advective-dispersive transport of salinity in a forming aquitard and in the underlying aquifer.";

#ifdef __cplusplus
extern "C"
#endif /* __cplusplus */

IfmResult RegisterModule(IfmModule pMod)
{
  if (IfmGetFeflowVersion (pMod) < IFM_REQUIRED_VERSION)
    return False;
  g_pMod = pMod;
  IfmRegisterModule (pMod, "SIMULATION", "PALEOSEA1D", "PaleoSea1D", 0x1000);
  IfmSetDescriptionString (pMod, szDesc);
  IfmSetCopyrightPath (pMod, "PaleoSea1D.txt");
  IfmSetHtmlPage (pMod, "PaleoSea1D.htm");
  IfmSetPrimarySource (pMod, "PaleoSea1D.cpp");
  IfmRegisterProc (pMod, "OnBeginDocument", 1, (IfmProc)OnBeginDocument);
  IfmRegisterProc (pMod, "OnEndDocument", 1, (IfmProc)OnEndDocument);
  IfmRegisterProc (pMod, "PreSimulation", 1, (IfmProc)PreSimulation);
  IfmRegisterProc (pMod, "PostSimulation", 1, (IfmProc)PostSimulation);
  IfmRegisterProc (pMod, "PreTimeStep", 1, (IfmProc)PreTimeStep);
  IfmRegisterProc (pMod, "PostTimeStep", 1, (IfmProc)PostTimeStep);
  return True;
}

//Global variables
int nnodes; //number of nodes (@PreSim)
int sedaccPID = -1; //sedaccum TS identifier
int swconcPID = -1; //swconc TS identifier
int quietmodePID = -1; //quietmode TS identifier (NEW!)
double defswconc = 0.0; //Default concentration for "seawater"
bool firstTSresumed; //true if simul. was resumed (restarted at time > 0)
bool inQuietMode = false; //false is the default; it is enabled when a time series named 'quietmode' is detected.

static void PreSimulation (IfmDocument pDoc)
{
  CPaleosea1d::FromHandle(pDoc)->PreSimulation (pDoc);
}
static void PostSimulation (IfmDocument pDoc)
{
  CPaleosea1d::FromHandle(pDoc)->PostSimulation (pDoc);
}
static void PreTimeStep (IfmDocument pDoc)
{
  CPaleosea1d::FromHandle(pDoc)->PreTimeStep (pDoc);
}
static void PostTimeStep (IfmDocument pDoc)
{
  CPaleosea1d::FromHandle(pDoc)->PostTimeStep (pDoc);
}

/* --- IFMREG_END --- */
#pragma endregion


static IfmResult OnBeginDocument (IfmDocument pDoc)
{
  if (IfmDocumentVersion (pDoc) < IFM_CURRENT_DOCUMENT_VERSION)
    return false;

  try {
    IfmDocumentSetUserData(pDoc, new CPaleosea1d(pDoc));
  }
  catch (...) {
    return false;
  }

  return true;
}

static void OnEndDocument (IfmDocument pDoc)
{
  delete CPaleosea1d::FromHandle(pDoc);
}

///////////////////////////////////////////////////////////////////////////
// Implementation of CPaleosea1d

// Constructor
CPaleosea1d::CPaleosea1d (IfmDocument pDoc)
  : m_pDoc(pDoc)
{
  /*
   * TODO: Add your own code here ...
   */
}

// Destructor
CPaleosea1d::~CPaleosea1d ()
{
  /*
   * TODO: Add your own code here ...
   */
}

// Obtaining class instance from document handle
CPaleosea1d* CPaleosea1d::FromHandle (IfmDocument pDoc)
{
  return reinterpret_cast<CPaleosea1d*>(IfmDocumentGetUserData(pDoc));
}

// Callbacks
void CPaleosea1d::PreSimulation (IfmDocument pDoc)
{
	IfmInfo(pDoc, "= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =");
	IfmInfo(pDoc, "  PaleoSea1D plugin for simulating 1D-z diffusion while stacking marine sediments  ");
	IfmInfo(pDoc, "                            (c) Marc Laurencelle, 2018                             ");
	//IfmInfo(pDoc, "* BETA tranfer rates @ elements found from 'chgnodes' nodal XY coordinates *");
	IfmInfo(pDoc, "<< preSimulation begins >>");
	int i;
	nnodes = IfmGetNumberOfNodes(pDoc);
	/*long rdid = IfmGetNodalRefDistrIdByName (pDoc, "chgnodes");
	if (rdid < 0) {
	IfmAlert (pDoc, NULL, "  OK  ", "The plugin requires a User Data nodal distrib. named chgnodes.");
	IfmSetSimulationControlFlag (pDoc, IfmCTL_ABORT);
	return;
	} */

	std::string PCname = "";
	char txtbuffer[180];
	i = IfmGetPowerCurve(pDoc, 0);
	sedaccPID = -1;
	swconcPID = -1;
	quietmodePID = -1;
	while (i>0) {
		PCname = IfmGetPowerComment(pDoc, i);
		if (PCname == "sedaccum") sedaccPID = i;
		if (PCname == "swconc") swconcPID = i;
		if (PCname == "quietmode") quietmodePID = i;
		//IfmInfo(pDoc, PCname.c_str());
		i = IfmGetPowerCurve(pDoc, i);
	}
	if (sedaccPID < 0) {
		IfmAlert(pDoc, NULL, "  OK  ", "The plugin requires a time series named sedaccum.");
		IfmSetSimulationControlFlag(pDoc, IfmCTL_ABORT);
		return;
	}
	if (swconcPID < 0) {
		sprintf_s(txtbuffer, 180, "The plugin detected no time series named swconc. A default constant value of %.3f g/L will be used.", defswconc / 1000);
		IfmWarning(pDoc, txtbuffer);
	}

	if (quietmodePID >= 0) {
		inQuietMode = true;
		IfmInfo(pDoc, "The Quiet Mode is enabled. Thus, very little information will be displayed during the simulation.");
	}

	/* TESTING Bcc type
	int bccmassT = IfmGetBccMassType(pDoc, 100);
	int bccmassV = IfmGetBccMassValue(pDoc, 100, bccmassT);
	sprintf_s(txtbuffer, 180, "TEST: bccmassT=%d, bccmassV=%f. At node #101.",bccmassT,bccmassV);
	IfmInfo(pDoc, txtbuffer);
	IfmSetSimulationControlFlag (pDoc, IfmCTL_ABORT);
	*/
	firstTSresumed = IfmGetAbsoluteSimulationTime(pDoc) > 0;

	if (firstTSresumed) {
		IfmInfo(pDoc, "continuing simulation starting with current b.c.'s (unchanged)");
	}
	//std::string InfoStr = "rslPID=" + std::to_string(rslPID);
	//IfmInfo(pDoc, InfoStr.c_str());
	//Vchgnodes.resize(nnodes);
	if (!inQuietMode) IfmInfo(pDoc, "calling malloc to create nodal-wise 1D arrays: NOTHING TO CREATE HERE.");
	//Pchgnodes = reinterpret_cast<bool*>(malloc(nnodes*sizeof(bool)));
	IfmInfo(pDoc, "<< preSimulation completed >>");
}

void CPaleosea1d::PostSimulation (IfmDocument pDoc)
{
	//free(Pchgnodes);
	if (!inQuietMode) IfmInfo(pDoc, "Freeing memory (NOTHING TO DO HERE).");
	IfmInfo(pDoc, "<< postSimulation done >> (paused, stopped or finished).");
}


void CPaleosea1d::PreTimeStep (IfmDocument pDoc)
{
	int i;
	//int cbcctype;
	//IfmBool cbccset;
	double zval;
	double saccval;
	double swconc;
	bool isinwater;
	double currtime; //absolute simulation time (in days)
	char txtbuffer[180];

	currtime = IfmGetAbsoluteSimulationTime(pDoc);
	saccval = IfmInterpolatePowerValue(pDoc, sedaccPID, currtime);
	swconc = swconcPID>0 ? IfmInterpolatePowerValue(pDoc, swconcPID, currtime) : defswconc;

	if (!inQuietMode) {
		sprintf_s(txtbuffer, 180, "updating b.c.'s for current time= %.2f d (%.2f a), with sedacc= %.3f m, swc= %.3f mg/L", currtime, currtime / 365.2425, saccval, swconc);
		IfmInfo(pDoc, txtbuffer);
	}

	for (i = 0; i < nnodes; i++) {
		//if(!Pchgnodes[i]) continue;
		zval = IfmGetY(pDoc, i);
		isinwater = zval > saccval;

		/* DEBUG nodal assignments
		cbcctype = IfmGetBccMassType(pDoc, i);
		cbccset = IfmIsBccMassSet(pDoc, i, 0);
		sprintf(txtbuffer,"time= %f, rsl= %f, node= %d, zval= %f, massbcct= %d, cbccset= %d", currtime, rslval, i+1, zval, cbcctype, cbccset);
		IfmInfo(pDoc, txtbuffer);
		*/

		//if(!firstTSresumed) { //TESTING isflooded != Pcurrfloodednodes[i]) {
		if (isinwater) {
			IfmSetBcMassTypeAndValueAtCurrentTime(pDoc, i, IfmBC_DIRICHLET, 0, swconc);
		}
		else {
			IfmSetBcMassTypeAndValueAtCurrentTime(pDoc, i, IfmBC_NONE, 0, 0);
		}
		//}
	}
	firstTSresumed = false;
}

void CPaleosea1d::PostTimeStep (IfmDocument pDoc)
{
	if (IfmIsTimeStepRejected(pDoc)) IfmInfo(pDoc, "! Current time step is rejected !");
}

