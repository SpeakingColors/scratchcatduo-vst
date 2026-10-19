// Plugin entry point.
#include "duo/Duo.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DuoProcessor(); }
