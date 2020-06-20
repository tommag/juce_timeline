/*
  ==============================================================================

	TimeCueManager.cpp
	Created: 6 Apr 2017 11:13:27am
	Author:  Ben

  ==============================================================================
*/

TimeCueManager::TimeCueManager() :
	BaseManager("Cues")
{
	comparator.compareFunc = &TimeCueManager::compareTime;
}

TimeCueManager::~TimeCueManager()
{

}

void TimeCueManager::addCueAt(float time)
{
	TimeCue * t = new TimeCue(time);
	BaseManager::addItem(t);
}

void TimeCueManager::reorderItems()
{
	items.sort(TimeCueManager::comparator, true);
	BaseManager::reorderItems();
}

Array<float> TimeCueManager::getAllCueTimes(float minTime, float maxTime, bool includeDisabled)
{
	Array<float> result;
	for (auto &tt : items)
	{
		if (!tt->enabled->boolValue() && !includeDisabled) continue;
		
		float t = tt->time->floatValue();
		if (maxTime > 0 && (t < minTime || t > maxTime)) continue;
		result.add(t);
	}
	return result;
}

float TimeCueManager::getNearestCueForTime(float time, bool includeDisabled)
{
	float result = time;
	if (items.size() == 0) return result;
    float diffTime = std::abs(time - items[0]->time->floatValue());
	result = items[0]->time->floatValue();
	int numItems = items.size();

	for (int i = 1; i < numItems; ++i)
	{
		if (!items[i]->enabled->boolValue() && !includeDisabled) continue;

		float newTime = items[i]->time->floatValue();
		float newDiff = std::abs(time - newTime);
		if (newDiff > diffTime) break;
		diffTime = newDiff;
		result = newTime;
	}
	return result;
}

Array<TimeCue*> TimeCueManager::getCuesInTimespan(float startTime, float endTime, bool includeDisabled)
{
	Array<TimeCue*> result;
	for (auto& tt : items)
	{
		if (!tt->enabled->boolValue() && !includeDisabled) continue;
		if (tt->time->floatValue() > startTime && tt->time->floatValue() <= endTime) result.add(tt);
	}
	return result;

}

float TimeCueManager::getNextCueForTime(float time, bool includeDisabled)
{
	int numItems = items.size();
	float result = time;
	for (int i = numItems - 1; i >= 0; i--)
	{
		if (!items[i]->enabled->boolValue() && !includeDisabled) continue;
		float t = items[i]->time->floatValue();
		if (t <= time) break;
		result = t;
	}

	return result;
}

float TimeCueManager::getPrevCueForTime(float time, float goToPreviousThreshold, bool includeDisabled)
{
	int numItems = items.size();
	float result = time;
	for (int i = 0; i < numItems; ++i)
	{
		if (!items[i]->enabled->boolValue() && !includeDisabled) continue;
		float t = items[i]->time->floatValue();
		if (t > time-goToPreviousThreshold) break;
		result = t;
	}

	return result;
}

int TimeCueManager::compareTime(TimeCue * t1, TimeCue * t2)
{
	if (t1->time->floatValue() < t2->time->floatValue()) return -1;
	else if (t1->time->floatValue() > t2->time->floatValue()) return 1;
	return 0;
}
