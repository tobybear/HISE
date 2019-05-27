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
 *   which also must be licensed for commercial applications:
 *
 *   http://www.juce.com
 *
 *   ===========================================================================
 */

namespace scriptnode
{
using namespace juce;
using namespace hise;







void DspHelpers::increaseBuffer(AudioSampleBuffer& b, int numChannels, int numSamples)
{
	if (numChannels != b.getNumChannels() ||
		b.getNumSamples() < numSamples)
	{
		b.setSize(numChannels, numSamples);
	}
}


scriptnode::DspHelpers::ParameterCallback DspHelpers::getFunctionFrom0To1ForRange(NormalisableRange<double> range, bool inverted, const ParameterCallback& originalFunction)
{
	if (RangeHelpers::isIdentity(range))
	{
		if (!inverted)
			return originalFunction;
		else
		{
			return [originalFunction](double normalisedValue)
			{
				originalFunction(1.0 - normalisedValue);
			};
		}
	}

	if (inverted)
	{
		return [range, originalFunction](double normalisedValue)
		{
			normalisedValue = 1.0 - normalisedValue;
			auto v = range.convertFrom0to1(normalisedValue);
			originalFunction(v);
		};
	}
	else
	{
		return [range, originalFunction](double normalisedValue)
		{
			auto v = range.convertFrom0to1(normalisedValue);
			originalFunction(v);
		};
	}
}

scriptnode::DspHelpers::ParameterCallback DspHelpers::wrapIntoConversionLambda(const Identifier& converterId, const ParameterCallback& originalFunction, NormalisableRange<double> range, bool inverted)
{
	using namespace ConverterIds;

	if (converterId == Identity)
		return getFunctionFrom0To1ForRange(range, inverted, originalFunction);
	if (converterId == Decibel2Gain)
	{
		return [originalFunction](double newValue)
		{
			double gain = Decibels::decibelsToGain(newValue);
			originalFunction(gain);
		};
	}
	if (converterId == Gain2Decibel)
	{
		return [originalFunction](double newValue)
		{
			double db = Decibels::gainToDecibels(newValue);
			originalFunction(db);
		};
	}
	if (converterId == SubtractFromOne)
	{
		auto withRange = getFunctionFrom0To1ForRange(range, false, originalFunction);

		return [withRange](double normedValue)
		{
			double v = jlimit(0.0, 1.0, 1.0 - normedValue);
			withRange(v);
		};
	}
	if (converterId == DryAmount)
	{
		return [originalFunction](double newValue)
		{
			auto invGain = jlimit(0.0, 1.0, 1.0 - newValue);
			auto invDb = Decibels::gainToDecibels(invGain);

			originalFunction(invDb);
		};
	}
	if (converterId == WetAmount)
	{
		return [originalFunction](double newValue)
		{
			auto db = Decibels::gainToDecibels(newValue);
			originalFunction(db);
		};
	}

	return getFunctionFrom0To1ForRange(range, inverted, originalFunction);
}


double DspHelpers::findPeak(ProcessData& data)
{
	double max = 0.0;

	for (auto channel : data)
	{
		auto r = FloatVectorOperations::findMinAndMax(channel, data.size);
		auto thisMax = jmax<float>(std::abs(r.getStart()), std::abs(r.getEnd()));
		max = jmax(max, (double)thisMax);
	}

	return max;
}

scriptnode::ProcessData ProcessData::copyTo(AudioSampleBuffer& buffer, int index)
{
	auto channelOffset = index * numChannels;

	int numChannelsToCopy = jmin(buffer.getNumChannels() - channelOffset, numChannels);
	int numSamplesToCopy = jmin(buffer.getNumSamples(), size);

	for (int i = 0; i < numChannelsToCopy; i++)
		buffer.copyFrom(i + channelOffset, 0, data[i], numSamplesToCopy);

	return referTo(buffer, index);
}


scriptnode::ProcessData ProcessData::referTo(AudioSampleBuffer& buffer, int index)
{
	ProcessData d;

	auto channelOffset = index * numChannels;

	d.numChannels = jmin(buffer.getNumChannels() - channelOffset, numChannels);
	d.size = jmin(buffer.getNumSamples(), size);
	d.data = buffer.getArrayOfWritePointers() + channelOffset;

	return d;
}

scriptnode::ProcessData& ProcessData::operator+=(const ProcessData& other)
{
	jassert(numChannels == other.numChannels);
	jassert(size == other.size);

	for (int i = 0; i < numChannels; i++)
		FloatVectorOperations::add(data[i], other.data[i], size);

	return *this;
}

}
