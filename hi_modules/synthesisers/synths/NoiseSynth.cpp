/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

namespace hise { using namespace juce;

ProcessorEditorBody* NoiseSynth::createEditor(ProcessorEditor *parentEditor)
{
#if USE_BACKEND

	return new EmptyProcessorEditorBody(parentEditor);

#else 

	ignoreUnused(parentEditor);
	jassertfalse;
	return nullptr;

#endif
}

#define USE_DC_FOR_DEBUGGING 1

void NoiseVoice::calculateBlock(int startSample, int numSamples)
{
	const int startIndex = startSample;
	const int samplesToCopy = numSamples;

#if USE_DC_FOR_DEBUGGING

	FloatVectorOperations::fill(voiceBuffer.getWritePointer(0, startSample), 1.0f, samplesToCopy);
	
#else
	while (--numSamples >= 0)
	{
		const float currentSample = getNextValue();

		// Stereo mode assumed
		voiceBuffer.setSample(1, startSample, currentSample);

		voiceUptime++;

		++startSample;
	}
#endif

	getOwnerSynth()->effectChain->renderVoice(voiceIndex, voiceBuffer, startIndex, samplesToCopy);

	if (auto modValues = getOwnerSynth()->getVoiceGainValues())
	{
		FloatVectorOperations::multiply(voiceBuffer.getWritePointer(0, startIndex), modValues + startIndex, samplesToCopy);
	}
	else
	{
		float gainValue = getOwnerSynth()->getConstantGainModValue();
		FloatVectorOperations::multiply(voiceBuffer.getWritePointer(0, startIndex), gainValue, samplesToCopy);
	}

	FloatVectorOperations::copy(voiceBuffer.getWritePointer(1, startIndex), voiceBuffer.getReadPointer(0, startIndex), samplesToCopy);
		
}

} // namespace hise
