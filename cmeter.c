/*
LADSPA plugin implementing an (initially simple) level meter.  Will aim to include peak, RMS, trough, crest factor, histogram(?!) and subjective loudness eventually!
Don't really see the point in implementing a stereo one of these - you should just be able to patch two of them to a stereo source in the host easily enough.
CME 2007-10-05
*/


#include <stdlib.h>
#define _GNU_SOURCE
#include <string.h>
#include <math.h>

#include "ladspa.h"



#define CMEMETER_LADSPA_ID	60


/* The internal ID numbers for the plugin's ports: */

#define CMEMETER_PORT_COUNT 5

// Huh? You have to define these in numerical order?!  C is too low-level for this stuff, IMHO.
#define METER_INPUT	0
#define METER_PEAK	1
#define METER_RMS	2
#define METER_TROUGH	3
#define METER_CREST	4






/* The structure used to hold port connection information and state
   (actually gain controls require no further state). */


typedef struct {
	LADSPA_Data * InputBuffer;
	LADSPA_Data * PeakLevel;
	LADSPA_Data * RMSLevel;
	LADSPA_Data * TroughLevel;
	LADSPA_Data * CrestFactor;
} Meter;


/* Construct a new plugin instance. */
LADSPA_Handle 
instantiateMeter(const LADSPA_Descriptor * Descriptor,
		     unsigned long             SampleRate) {
	return malloc(sizeof(Meter));
}


/* Connect a port to a data location. */
void 
connectPortToMeter(LADSPA_Handle Instance,
		       unsigned long Port,
		       LADSPA_Data * DataLocation) {

	Meter * psMeter;

	psMeter = (Meter *)Instance;
	switch (Port) {
		case METER_INPUT:
			psMeter->InputBuffer = DataLocation;
			break;
		case METER_PEAK:
			psMeter->PeakLevel = DataLocation;
			break;
		case METER_RMS:
			psMeter->RMSLevel = DataLocation;
			break;
		case METER_TROUGH:
			psMeter->TroughLevel = DataLocation;
			break;
		case METER_CREST:
			psMeter->CrestFactor = DataLocation;
			break;
	}
}



void 
runMeter(LADSPA_Handle Instance,
		 unsigned long SampleCount) {
  
	LADSPA_Data * Input;
	// All meter outputs in dB re. 1.0.
	LADSPA_Data	PeakLevel;
	LADSPA_Data	RMSLevel;
	//LADSPA_Data	TroughLevel;
	//LADSPA_Data	CrestFactor;

	LADSPA_Data	RMSSumOfSquares = 0.0;
	//LADSPA_Data	RMSWindowBuffer[SampleCount]

	Meter * psMeter;
	unsigned long SampleIndex;

	psMeter = (Meter *)Instance;

	// Keep track of the largest and smallest sample encoured within the current window
	LADSPA_Data	MaxSample = 0.0;
	LADSPA_Data	MinSample = 1.0;

	Input = psMeter->InputBuffer;
	//PeakLevel = *(psMeter->PeakLevel);

	// Process the sample buffer:
	for (SampleIndex = 0; SampleIndex < SampleCount; SampleIndex++)
	{
		// Update the largest and smallest absolute sample value encountered (if this window has a new record-holder):
		if (fabs(*Input) < MinSample)	MinSample = fabs(*Input);
		if (fabs(*Input) > MaxSample)	MaxSample = fabs(*Input);

		// Update the numerator for RMS calculation:
		RMSSumOfSquares += (*Input) * (*Input);

		Input++;
	}

	// Output the calculated values to the meter ports:
	// We save PeakLevel and RMSLevel to make the crest factor calculation a bit cheaper (avoid recalculating)
	PeakLevel = 20 * log10(MaxSample); *psMeter->PeakLevel = PeakLevel;
	RMSLevel = 20 * log10(sqrt(RMSSumOfSquares / SampleCount)); *psMeter->RMSLevel = RMSLevel;
	*psMeter->TroughLevel = 20 * log10(MinSample);
	*psMeter->CrestFactor = PeakLevel - RMSLevel;
}






/* Throw away a simple delay line. */
void 
cleanupMeter(LADSPA_Handle Instance) {
	free(Instance);
}



LADSPA_Descriptor * g_psMeterDescriptor = NULL;




void 
_init() {

	char ** pcPortNames;
	LADSPA_PortDescriptor * piPortDescriptors;
	LADSPA_PortRangeHint * psPortRangeHints;

	g_psMeterDescriptor = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));

  
		g_psMeterDescriptor->UniqueID = CMEMETER_LADSPA_ID;
		g_psMeterDescriptor->Label = strdup("cme_meter");
		g_psMeterDescriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
		g_psMeterDescriptor->Name = strdup("Meter (CME)");
		g_psMeterDescriptor->Maker = strdup("Chris Edwards");
		g_psMeterDescriptor->Copyright = strdup("None");

		g_psMeterDescriptor->PortCount = CMEMETER_PORT_COUNT;
		piPortDescriptors = (LADSPA_PortDescriptor *)calloc(CMEMETER_PORT_COUNT, sizeof(LADSPA_PortDescriptor));
		g_psMeterDescriptor->PortDescriptors = (const LADSPA_PortDescriptor *)piPortDescriptors;
		piPortDescriptors[METER_INPUT] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
		piPortDescriptors[METER_PEAK] = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
		piPortDescriptors[METER_RMS] = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
		piPortDescriptors[METER_TROUGH] = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
		piPortDescriptors[METER_CREST] = LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
		
		pcPortNames = (char **)calloc(CMEMETER_PORT_COUNT, sizeof(char *));
		g_psMeterDescriptor->PortNames = (const char **)pcPortNames;
		pcPortNames[METER_INPUT] = strdup("Input");
		pcPortNames[METER_PEAK] = strdup("Peak level (dB)");
		pcPortNames[METER_RMS] = strdup("RMS level (dB)");
		pcPortNames[METER_TROUGH] = strdup("Trough level (dB)");
		pcPortNames[METER_CREST] = strdup("Crest factor (dB)");
		psPortRangeHints = ((LADSPA_PortRangeHint *) calloc(CMEMETER_PORT_COUNT, sizeof(LADSPA_PortRangeHint)));
		g_psMeterDescriptor->PortRangeHints = (const LADSPA_PortRangeHint *)psPortRangeHints;


		psPortRangeHints[METER_PEAK].HintDescriptor = (
		    	LADSPA_HINT_BOUNDED_BELOW | 
			LADSPA_HINT_BOUNDED_ABOVE | 
			LADSPA_HINT_DEFAULT_0
		);
		psPortRangeHints[METER_PEAK].LowerBound = -120;
		psPortRangeHints[METER_PEAK].UpperBound = 0;


		psPortRangeHints[METER_RMS].HintDescriptor = (
		    	LADSPA_HINT_BOUNDED_BELOW | 
			LADSPA_HINT_BOUNDED_ABOVE | 
			LADSPA_HINT_DEFAULT_0
		);
		psPortRangeHints[METER_RMS].LowerBound = -120;
		psPortRangeHints[METER_RMS].UpperBound = 0;


		psPortRangeHints[METER_TROUGH].HintDescriptor = (
		   	LADSPA_HINT_BOUNDED_BELOW | 
			LADSPA_HINT_BOUNDED_ABOVE | 
			LADSPA_HINT_DEFAULT_0
		);
		psPortRangeHints[METER_TROUGH].LowerBound = -120;
		psPortRangeHints[METER_TROUGH].UpperBound = 0;


		psPortRangeHints[METER_CREST].HintDescriptor = (
		    	LADSPA_HINT_BOUNDED_BELOW | 
			LADSPA_HINT_BOUNDED_ABOVE | 
			LADSPA_HINT_DEFAULT_0
		);
		psPortRangeHints[METER_CREST].LowerBound = 0;
		psPortRangeHints[METER_CREST].UpperBound = 30;


		psPortRangeHints[METER_INPUT].HintDescriptor = 0;


		g_psMeterDescriptor->instantiate = instantiateMeter;
		g_psMeterDescriptor->connect_port = connectPortToMeter;
		g_psMeterDescriptor->activate = NULL;
		g_psMeterDescriptor->run = runMeter;
		g_psMeterDescriptor->run_adding = NULL;
		g_psMeterDescriptor->set_run_adding_gain = NULL;
		g_psMeterDescriptor->deactivate = NULL;
		g_psMeterDescriptor->cleanup = cleanupMeter;


}


void
deleteDescriptor(LADSPA_Descriptor * psDescriptor) {
  unsigned long lIndex;
	if (psDescriptor) {
		free((char *)psDescriptor->Label);
		free((char *)psDescriptor->Name);
		free((char *)psDescriptor->Maker);
		free((char *)psDescriptor->Copyright);
		free((LADSPA_PortDescriptor *)psDescriptor->PortDescriptors);
		for (lIndex = 0; lIndex < psDescriptor->PortCount; lIndex++)
			free((char *)(psDescriptor->PortNames[lIndex]));
		free((char **)psDescriptor->PortNames);
		free((LADSPA_PortRangeHint *)psDescriptor->PortRangeHints);
		free(psDescriptor);
	}
}


void
_fini() {
	deleteDescriptor(g_psMeterDescriptor);
}


/* Return a descriptor of the requested plugin type. There are two
   plugin types available in this library (pan and balance). */
const LADSPA_Descriptor * 
ladspa_descriptor(unsigned long Index) {
	/* Return the requested descriptor or null if the index is out of range. */
	switch (Index) {
	case 0:
		return g_psMeterDescriptor;
	default:
		return NULL;
	}
}


