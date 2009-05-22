/* amp.c

   Free software by Richard W.E. Furse. Do with as you will. No
   warranty.

   Modified 2007-09-29 by Chris Edwards; just trying to create a basic LADSPA plugin.

   This LADSPA plugin provides simple mono and stereo amplifiers, now with gain controls in decibels (dB) and an extended range (-120..+120 dB, which should cover all possible normal uses).

   Doesn't implement independent gain and mute controls for L and R in the stereo version (no sense in implementing an entire mixer!).

   This file has poor memory protection. Failures during malloc() will
   not recover nicely. */

/*****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>

/*****************************************************************************/

#include "ladspa.h"

/*****************************************************************************/

#define CMEAMP_MONO_LADSPA_ID	48
#define CMEAMP_STEREO_LADSPA_ID 49

#define CMEAMP_MONO_PORT_COUNT 4
#define CMEAMP_STEREO_PORT_COUNT 6

/* The internal ID numbers for the plugin's ports: */

// Huh? You have to define these in numerical order?!  C is too low-level for this stuff, IMHO.
#define AMP_GAIN 0
#define AMP_MUTE 1
#define AMP_INPUT1  2
#define AMP_OUTPUT1 3
#define AMP_INPUT2  4
#define AMP_OUTPUT2 5

/*****************************************************************************/

/* The structure used to hold port connection information and state
   (actually gain controls require no further state). */


typedef struct {
	LADSPA_Data * m_pfControlValue;
	LADSPA_Data * m_pfMuteValue;	// CME
	LADSPA_Data * m_pfInputBuffer1;
	LADSPA_Data * m_pfOutputBuffer1;
	LADSPA_Data * m_pfInputBuffer2;  /* (Not used for mono) */
	LADSPA_Data * m_pfOutputBuffer2; /* (Not used for mono) */
} Amplifier;


/* Construct a new plugin instance. */
LADSPA_Handle 
instantiateAmplifier(const LADSPA_Descriptor * Descriptor,
		     unsigned long             SampleRate) {
	return malloc(sizeof(Amplifier));
}


/* Connect a port to a data location. */
void 
connectPortToAmplifier(LADSPA_Handle Instance,
		       unsigned long Port,
		       LADSPA_Data * DataLocation) {

	Amplifier * psAmplifier;

	psAmplifier = (Amplifier *)Instance;
	switch (Port) {
		case AMP_GAIN:
			psAmplifier->m_pfControlValue = DataLocation;
			break;
		case AMP_MUTE:
			psAmplifier->m_pfMuteValue = DataLocation;
			break;
		case AMP_INPUT1:
			psAmplifier->m_pfInputBuffer1 = DataLocation;
			break;
		case AMP_OUTPUT1:
			psAmplifier->m_pfOutputBuffer1 = DataLocation;
			break;
		case AMP_INPUT2:
			/* (This should only happen for stereo.) */
			psAmplifier->m_pfInputBuffer2 = DataLocation;
			break;
		case AMP_OUTPUT2:
			/* (This should only happen for stereo.) */
			psAmplifier->m_pfOutputBuffer2 = DataLocation;
			break;
	}
}



void 
runMonoAmplifier(LADSPA_Handle Instance,
		 unsigned long SampleCount) {
  
	LADSPA_Data * pfInput;
	LADSPA_Data * pfOutput;
	LADSPA_Data fGain;	// CME: now in dB
	LADSPA_Data fMute;	// CME: added mute control.
	LADSPA_Data GainFactor;	// Store it to reduce CPU load.
	Amplifier * psAmplifier;
	unsigned long lSampleIndex;

	psAmplifier = (Amplifier *)Instance;

	pfInput = psAmplifier->m_pfInputBuffer1;
	pfOutput = psAmplifier->m_pfOutputBuffer1;
	fGain = *(psAmplifier->m_pfControlValue);
	GainFactor = pow(10, fGain / 20.0);
	fMute = *(psAmplifier->m_pfMuteValue);

	if (fMute == 1)	// If mute switch enabled, output silence
		for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++)
			*(pfOutput++) = 0.0;
	else	// otherwise scale the input according to the gain control
		for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++)
			*(pfOutput++) = *(pfInput++) * GainFactor;
	// I nested the for loops within the conditional rather than the other way round to avoid having to check the state of fMute every iteration.
}



void 
runStereoAmplifier(LADSPA_Handle Instance,
		   unsigned long SampleCount) {
  
	LADSPA_Data * pfInput;
	LADSPA_Data * pfOutput;
	LADSPA_Data fGain;	// CME
	LADSPA_Data fMute;	// CME
	LADSPA_Data GainFactor;	// CME
	Amplifier * psAmplifier;
	unsigned long lSampleIndex;

	psAmplifier = (Amplifier *)Instance;

	fGain = *(psAmplifier->m_pfControlValue);
	GainFactor = pow(10.0, fGain / 20.0);
	fMute = *(psAmplifier->m_pfMuteValue);

	// Process L buffer:
	pfInput = psAmplifier->m_pfInputBuffer1;
	pfOutput = psAmplifier->m_pfOutputBuffer1;
	if (fMute == 1)
		for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++)
			*(pfOutput++) = 0.0;
	else
		for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++) 
			*(pfOutput++) = *(pfInput++) * GainFactor;

	// Process R buffer:
	pfInput = psAmplifier->m_pfInputBuffer2;
	pfOutput = psAmplifier->m_pfOutputBuffer2;
	if (fMute == 1)
		for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++) 
			*(pfOutput++) = 0.0;
	else
		for (lSampleIndex = 0; lSampleIndex < SampleCount; lSampleIndex++) 
			*(pfOutput++) = *(pfInput++) * GainFactor;
}


/* Throw away a simple delay line. */
void 
cleanupAmplifier(LADSPA_Handle Instance) {
	free(Instance);
}



LADSPA_Descriptor * g_psMonoDescriptor = NULL;
LADSPA_Descriptor * g_psStereoDescriptor = NULL;



/* _init() is called automatically when the plugin library is first
   loaded. */
void 
_init() {

	char ** pcPortNames;
	LADSPA_PortDescriptor * piPortDescriptors;
	LADSPA_PortRangeHint * psPortRangeHints;

	g_psMonoDescriptor = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));
	g_psStereoDescriptor = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));

	if (g_psMonoDescriptor) {
  
		g_psMonoDescriptor->UniqueID = CMEAMP_MONO_LADSPA_ID;
		g_psMonoDescriptor->Label = strdup("gain_mono");
		g_psMonoDescriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
		g_psMonoDescriptor->Name = strdup("Gain (dB), Mono, with mute (CME)");
		g_psMonoDescriptor->Maker = strdup("Chris Edwards");
		g_psMonoDescriptor->Copyright = strdup("None");

		g_psMonoDescriptor->PortCount = CMEAMP_MONO_PORT_COUNT;
		piPortDescriptors = (LADSPA_PortDescriptor *)calloc(CMEAMP_MONO_PORT_COUNT, sizeof(LADSPA_PortDescriptor));
		g_psMonoDescriptor->PortDescriptors = (const LADSPA_PortDescriptor *)piPortDescriptors;
		piPortDescriptors[AMP_GAIN] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		piPortDescriptors[AMP_MUTE] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		piPortDescriptors[AMP_INPUT1] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
		piPortDescriptors[AMP_OUTPUT1] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
		pcPortNames = (char **)calloc(CMEAMP_MONO_PORT_COUNT, sizeof(char *));
		g_psMonoDescriptor->PortNames = (const char **)pcPortNames;
		pcPortNames[AMP_GAIN] = strdup("Gain");
		pcPortNames[AMP_MUTE] = strdup("Mute");
		pcPortNames[AMP_INPUT1]= strdup("Input");
		pcPortNames[AMP_OUTPUT1]= strdup("Output");
		psPortRangeHints = ((LADSPA_PortRangeHint *) calloc(CMEAMP_MONO_PORT_COUNT, sizeof(LADSPA_PortRangeHint)));
		g_psMonoDescriptor->PortRangeHints = (const LADSPA_PortRangeHint *)psPortRangeHints;

		psPortRangeHints[AMP_GAIN].HintDescriptor = (
		    	LADSPA_HINT_BOUNDED_BELOW | 
			LADSPA_HINT_BOUNDED_ABOVE | 
			LADSPA_HINT_DEFAULT_0
		);
		psPortRangeHints[AMP_GAIN].LowerBound = -120;
		psPortRangeHints[AMP_GAIN].UpperBound = 120;

		psPortRangeHints[AMP_MUTE].HintDescriptor = (LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_0);

		psPortRangeHints[AMP_INPUT1].HintDescriptor = 0;
		psPortRangeHints[AMP_OUTPUT1].HintDescriptor = 0;

		g_psMonoDescriptor->instantiate = instantiateAmplifier;
		g_psMonoDescriptor->connect_port = connectPortToAmplifier;
		g_psMonoDescriptor->activate = NULL;
		g_psMonoDescriptor->run = runMonoAmplifier;
		g_psMonoDescriptor->run_adding = NULL;
		g_psMonoDescriptor->set_run_adding_gain = NULL;
		g_psMonoDescriptor->deactivate = NULL;
		g_psMonoDescriptor->cleanup = cleanupAmplifier;
	}
  
	if (g_psStereoDescriptor) {
		g_psStereoDescriptor->UniqueID = CMEAMP_STEREO_LADSPA_ID;
		g_psStereoDescriptor->Label = strdup("gain_stereo");
		g_psStereoDescriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
		g_psStereoDescriptor->Name = strdup("Gain (dB), Stereo, with mute (CME)");
		g_psStereoDescriptor->Maker = strdup("Chris Edwards");
		g_psStereoDescriptor->Copyright = strdup("None");

		g_psStereoDescriptor->PortCount = CMEAMP_STEREO_PORT_COUNT;
		piPortDescriptors = (LADSPA_PortDescriptor *)calloc(CMEAMP_STEREO_PORT_COUNT, sizeof(LADSPA_PortDescriptor));
		g_psStereoDescriptor->PortDescriptors = (const LADSPA_PortDescriptor *)piPortDescriptors;
		piPortDescriptors[AMP_GAIN] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		piPortDescriptors[AMP_MUTE] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		piPortDescriptors[AMP_INPUT1] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
		piPortDescriptors[AMP_OUTPUT1] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
		piPortDescriptors[AMP_INPUT2] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
		piPortDescriptors[AMP_OUTPUT2] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
		pcPortNames = (char **)calloc(CMEAMP_STEREO_PORT_COUNT, sizeof(char *));
		g_psStereoDescriptor->PortNames = (const char **)pcPortNames;
		
		pcPortNames[AMP_GAIN]= strdup("Gain");
		pcPortNames[AMP_MUTE] = strdup("Mute");
		pcPortNames[AMP_INPUT1] = strdup("Input (Left)");
		pcPortNames[AMP_OUTPUT1] = strdup("Output (Left)");
		pcPortNames[AMP_INPUT2] = strdup("Input (Right)");
		pcPortNames[AMP_OUTPUT2] = strdup("Output (Right)");

		psPortRangeHints = ((LADSPA_PortRangeHint *) calloc(CMEAMP_STEREO_PORT_COUNT, sizeof(LADSPA_PortRangeHint)));
		g_psStereoDescriptor->PortRangeHints = (const LADSPA_PortRangeHint *)psPortRangeHints;

		psPortRangeHints[AMP_GAIN].HintDescriptor = (
		    	LADSPA_HINT_BOUNDED_BELOW | 
			LADSPA_HINT_BOUNDED_ABOVE | 
			LADSPA_HINT_DEFAULT_0
			);
		psPortRangeHints[AMP_GAIN].LowerBound = -120;
		psPortRangeHints[AMP_GAIN].UpperBound = 120;

		psPortRangeHints[AMP_MUTE].HintDescriptor = (LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_0);

		psPortRangeHints[AMP_INPUT1].HintDescriptor = 0;
		psPortRangeHints[AMP_OUTPUT1].HintDescriptor = 0;
		psPortRangeHints[AMP_INPUT2].HintDescriptor = 0;
		psPortRangeHints[AMP_OUTPUT2].HintDescriptor = 0;

		g_psStereoDescriptor->instantiate = instantiateAmplifier;
		g_psStereoDescriptor->connect_port = connectPortToAmplifier;
		g_psStereoDescriptor->activate = NULL;
		g_psStereoDescriptor->run = runStereoAmplifier;
		g_psStereoDescriptor->run_adding = NULL;
		g_psStereoDescriptor->set_run_adding_gain = NULL;
		g_psStereoDescriptor->deactivate = NULL;
		g_psStereoDescriptor->cleanup = cleanupAmplifier;
	}
}

/*****************************************************************************/

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

/*****************************************************************************/

/* _fini() is called automatically when the library is unloaded. */
void
_fini() {
	deleteDescriptor(g_psMonoDescriptor);
	deleteDescriptor(g_psStereoDescriptor);
}

/*****************************************************************************/

/* Return a descriptor of the requested plugin type. There are two
   plugin types available in this library (mono and stereo). */
const LADSPA_Descriptor * 
ladspa_descriptor(unsigned long Index) {
	/* Return the requested descriptor or null if the index is out of range. */
	switch (Index) {
	case 0:
		return g_psMonoDescriptor;
	case 1:
		return g_psStereoDescriptor;
	default:
		return NULL;
	}
}

/*****************************************************************************/

/* EOF */
