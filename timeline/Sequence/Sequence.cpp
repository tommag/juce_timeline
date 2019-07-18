#include "Sequence.h"
/*
  ==============================================================================

    Sequence.cpp
    Created: 28 Oct 2016 8:13:19pm
    Author:  bkupe

  ==============================================================================
*/

Sequence::Sequence() :
	BaseItem("Sequence",true),
	currentManager(nullptr),
	hiResAudioTime(0),
	sampleRate(44100),
	isSeeking(false),
	isBeingEdited(false),
	sequenceNotifier(10)
{
	itemDataType = "Sequence";
	helpID = "Sequence";

	isPlaying = addBoolParameter("Is Playing", "Is the sequence playing ?", false);
	isPlaying->setControllableFeedbackOnly(true);
	isPlaying->isSavable = false;
	isPlaying->hideInEditor = true;

	playTrigger = addTrigger("Play", "Play the sequence");
	stopTrigger = addTrigger("Stop", "Stops the sequence and set the current time at 0.");
	finishTrigger = addTrigger("Finish", "When the sequence reached naturally its end, and there is no loop");
	finishTrigger->hideInEditor = true;
	pauseTrigger = addTrigger("Pause", "Pause the sequence and keep the current time as is.");
	togglePlayTrigger = addTrigger("TogglePlay", "Toggle between play/pause or play/stop depending on sequence settings");
	
	float initTotalTime = 30; //default to 30 seconds, may be in general preferences later

	startAtLoad = addBoolParameter("Play at Load", "If selected, the sequence will start playing just after loading the file", false);

	currentTime = addFloatParameter("Current Time", "Current position in time of this sequence", 0, 0, initTotalTime);
	currentTime->defaultUI = FloatParameter::TIME;
	currentTime->isSavable = false;

	totalTime = addFloatParameter("Total Time", "Total time of this sequence, in seconds", initTotalTime, minSequenceTime);
	totalTime->defaultUI = FloatParameter::TIME;

	loopParam = addBoolParameter("Loop", "Whether the sequence plays again from the start when reached the end while playing", false);
	playSpeed = addFloatParameter("Play Speed", "Playing speed factor, 1 is normal speed, 2 is double speed and 0.5 is half speed",1,0.01f);
	fps = addIntParameter("FPS", "Frame Per Second.\nDefines the number of times per seconds the sequence is evaluated, the higher the value is, the more previse the calculation will be.\n \
									This setting also sets how many messages per seconds are sent from layer with automations.", 50, 1,500);
	
	
	prevCue = addTrigger("Prev Cue", "Jump to previous cue, if previous cue is less than 1 sec before, jump to the one before that.");
	nextCue = addTrigger("Next Cue", "Jump to the next cue");

	viewStartTime = addFloatParameter("View start time", "Start time of the view", 0, 0, initTotalTime - minSequenceTime);
	viewStartTime->hideInEditor = true;

	viewEndTime = addFloatParameter("View end time", "End time of the view", initTotalTime, minSequenceTime, initTotalTime);
	viewEndTime->hideInEditor = true;

	layerManager.reset(new SequenceLayerManager(this));
	addChildControllableContainer(layerManager.get());

	cueManager.reset(new TimeCueManager());
	addChildControllableContainer(cueManager.get());
	cueManager->hideInEditor = true;

	listUISize->setValue(5);

}

Sequence::~Sequence()
{
	clearItem();
}

void Sequence::clearItem()
{
	setAudioDeviceManager(nullptr);

	stopTimer();
	stopTrigger->trigger();

	if (Engine::mainEngine != nullptr) Engine::mainEngine->removeEngineListener(this);
}

void Sequence::setCurrentTime(float time, bool forceOverPlaying, bool seekMode)
{
	time = jlimit<float>(0, totalTime->floatValue(), time);

	if (isPlaying->boolValue() && !forceOverPlaying) return;

	isSeeking = seekMode;
	if (timeIsDrivenByAudio())
	{
		hiResAudioTime = time;
		if (!isPlaying->boolValue() || isSeeking) currentTime->setValue(time);
	}
	else
	{
		currentTime->setValue(time);
	}
	isSeeking = false;
}

void Sequence::setBeingEdited(bool value)
{
	if (isBeingEdited == value) return;
	isBeingEdited = value;
	sequenceListeners.call(&SequenceListener::sequenceEditingStateChanged, this);
	sequenceNotifier.addMessage(new SequenceEvent(SequenceEvent::EDITING_STATE_CHANGED, this));
}

bool Sequence::paste()
{
	Array<SequenceLayer *> p = layerManager->addItemsFromClipboard(false);
	if (p.isEmpty()) return BaseItem::paste();
	return true;
}

void Sequence::setAudioDeviceManager(AudioDeviceManager * manager)
{
	if (currentManager == manager) return;

	if (currentManager != nullptr) currentManager->removeAudioCallback(this);

	currentManager = manager;

	if (currentManager != nullptr) currentManager->addAudioCallback(this);


	//resync values between audio/non-audio driving variables
	hiResAudioTime = (double)currentTime->floatValue();
	prevMillis = Time::getMillisecondCounterHiRes();
	
	sequenceListeners.call(&SequenceListener::sequenceMasterAudioModuleChanged, this);
}

void Sequence::updateSampleRate()
{
	AudioDeviceManager::AudioDeviceSetup s;
	if (currentManager != nullptr) currentManager->getAudioDeviceSetup(s);
	if(s.sampleRate != 0) sampleRate = s.sampleRate;
}

bool Sequence::timeIsDrivenByAudio()
{
	return currentManager != nullptr;
}

var Sequence::getJSONData()
{
	var data = BaseItem::getJSONData();
	var layerData = layerManager->getJSONData();
	if(!layerData.isVoid()) data.getDynamicObject()->setProperty("layerManager", layerData );
	var cueData = cueManager->getJSONData();
	if(!cueData.isVoid()) data.getDynamicObject()->setProperty("cueManager", cueData);
	if(isBeingEdited) data.getDynamicObject()->setProperty("editing", true);
	return data;
}

void Sequence::loadJSONDataInternal(var data)
{
	BaseItem::loadJSONDataInternal(data);
	layerManager->loadJSONData(data.getProperty("layerManager", var()));
	cueManager->loadJSONData(data.getProperty("cueManager", var()));
	isBeingEdited = data.getProperty("editing", false);

	if (Engine::mainEngine->isLoadingFile)
	{
		Engine::mainEngine->addEngineListener(this);
	}
	
}

void Sequence::onContainerParameterChangedInternal(Parameter * p)
{
	if (p == enabled)
	{

	}
	else if (p == currentTime)
	{
		if ((!isPlaying->boolValue() || isSeeking) && timeIsDrivenByAudio()) hiResAudioTime = currentTime->floatValue(); 
		sequenceListeners.call(&SequenceListener::sequenceCurrentTimeChanged, this, (float)prevTime, isPlaying->boolValue());
		prevTime = currentTime->floatValue();
	}
	else if (p == totalTime)
	{
		float minViewTime = jmax(minSequenceTime, totalTime->floatValue() / 100.f); //small hack to avoid UI hang when zooming too much

		currentTime->setRange(0, totalTime->floatValue());
		viewStartTime->setRange(0, totalTime->floatValue() - minViewTime);
		viewEndTime->setRange(viewStartTime->floatValue() + minViewTime, totalTime->floatValue());
		sequenceListeners.call(&SequenceListener::sequenceTotalTimeChanged, this);
	}
	else if (p == isPlaying)
	{
		if (isPlaying->boolValue())
		{
			prevMillis = Time::getMillisecondCounterHiRes();
			prevTime = currentTime->floatValue();
			startTimer(1000/fps->intValue());
		}
		else
		{
			stopTimer();
		}

		sequenceListeners.call(&SequenceListener::sequencePlayStateChanged, this);


	} else if (p == fps)
	{
		if (isPlaying->boolValue())
		{
			stopTimer();
			startTimer(1000/fps->intValue());
		}
	}
	else if (p == playSpeed)
	{
		sequenceListeners.call(&SequenceListener::sequencePlaySpeedChanged, this);
	}
	else if (p == viewStartTime)
	{
		float minViewTime = jmax(minSequenceTime, totalTime->floatValue() / 100.f); //small hack to avoid UI hang when zooming too much
		viewEndTime->setRange(viewStartTime->floatValue() + minViewTime, totalTime->floatValue()); //Should be a range value
	}
}

void Sequence::onContainerTriggerTriggered(Trigger * t)
{
	if (t == playTrigger)
	{
		if (currentTime->floatValue() >= totalTime->floatValue()) currentTime->setValue(0); //if reached the end when hit play, go to 0
		isPlaying->setValue(true);
	} else if(t == stopTrigger)
	{
		isPlaying->setValue(false);
		currentTime->setValue(0);
	} else if (t == pauseTrigger)
	{
		isPlaying->setValue(false);
	} else if (t == finishTrigger)
	{
		isPlaying->setValue(false);
	} else if (t == togglePlayTrigger)
	{
		if (isPlaying->boolValue()) pauseTrigger->trigger();
		else playTrigger->trigger();

	} else if (t == prevCue)
	{
		setCurrentTime(cueManager->getPrevCueForTime(currentTime->floatValue(), 1));
	} else if (t == nextCue)
	{
		setCurrentTime(cueManager->getNextCueForTime(currentTime->floatValue()));
	}
}

void Sequence::hiResTimerCallback()
{
	if (!isPlaying->boolValue()) return;

	double targetTime = 0;
    
    //DBG("Hi res callback here");
    
    if (timeIsDrivenByAudio())
	{
		targetTime = hiResAudioTime;
        currentTime->setValue(hiResAudioTime);
	}
	else
	{
		double millis = Time::getMillisecondCounterHiRes();
		double deltaTime = (millis - prevMillis) / 1000;
		targetTime = currentTime->floatValue() + deltaTime * playSpeed->floatValue();
		currentTime->setValue(targetTime);
		prevMillis = millis;
	}

	if (targetTime >= totalTime->floatValue())
	{
		if (loopParam->boolValue())
		{
			float offset = targetTime - totalTime->floatValue();
			sequenceListeners.call(&SequenceListener::sequenceLooped, this);
			//setCurrentTime(0); //to change in trigger layer to avoid doing that
			prevTime = 0;
			setCurrentTime(offset);
		}
		else finishTrigger->trigger();
	}
}

void Sequence::endLoadFile()
{
	Engine::mainEngine->removeEngineListener(this);
	if (isBeingEdited) selectThis();

	if (startAtLoad->boolValue())
	{
		playTrigger->trigger();
	}
}


void Sequence::audioDeviceIOCallback(const float ** , int , float ** outputChannelData, int numOutputChannels, int numSamples)
{
	for(int i=0;i<numOutputChannels;i++) FloatVectorOperations::clear(outputChannelData[i], numSamples);

	
	if (isPlaying->boolValue()) hiResAudioTime += (numSamples / sampleRate) * playSpeed->floatValue();
}

void Sequence::audioDeviceAboutToStart(AudioIODevice *)
{
	updateSampleRate();
}

void Sequence::audioDeviceStopped()
{
}
