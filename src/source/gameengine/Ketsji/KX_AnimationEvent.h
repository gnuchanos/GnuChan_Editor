/*
* ***** BEGIN GPL LICENSE BLOCK *****
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* Contributor(s): Range Engine.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file KX_AnimationEvent.h
 *  \ingroup ketsji
 */

#ifndef __KX_ANIMATION_EVENT_H__
#define __KX_ANIMATION_EVENT_H__

#include "EXP_Value.h"

class KX_AnimationEvent : public EXP_Value
{
	Py_Header
protected:
	std::string			m_name;

private:
	const char *m_actionName;
	/* bAnimationEventTriggers has converted here now, std::pair<frame_trigger, custom_argument_call> */
	std::vector<std::pair<int, std::string>> *m_triggers;
	std::vector<int> *m_alreadyTriggered;

	/// is used to call the events in KX_Scene->UpdateAnimations, it is safe to do the python calls there.
	std::vector<std::string> *m_events_toCall;

	const char *m_pythonEvent;

	/// For logic bricks
	int m_lastTriggerIndex;

#ifdef WITH_PYTHON
	PyObject *m_pyEventFunction;
#endif // WITH_PYTHON

public:
	KX_AnimationEvent(const char *actionName,
                    std::vector<std::pair<int, std::string>> *triggers,
                    const char *pythonEvent);
	KX_AnimationEvent(const KX_AnimationEvent *other);
	virtual ~KX_AnimationEvent();

	virtual std::string GetName();

	unsigned int GetTriggerCount() const;
	virtual int GetTrigger(int index) const;
	virtual const char *GetCustomArg(int index) const;

	virtual std::string GetActionName();
	virtual std::vector<std::pair<int, std::string>> *GetTriggers();

	void SetTriggers(std::vector<std::pair<int, std::string>> *triggers);

	virtual std::vector<int> GetAlreadyTriggereds() const;
	void SetAlreadyTriggered(int frameTrigger);
	void ClearAlreadyTriggereds();
	virtual const char *GetPythonEventName();

	/// get the events iterate through KX_Scene->UpdateAnimations, then clean up the vector.
	std::vector<std::string> GetEventsToCall() const;
	void SetEventToCall(const char *customArg);
	void ClearEventsToCall();

	/// For logic bricks
	virtual const int GetLastTriggerIndex();
	virtual void SetLastTriggerIndex(const int triggerIndex);

#ifdef WITH_PYTHON
	PyObject *GetPyEventFunction();

	static PyObject *pyattr_get_triggers(EXP_PyObjectPlus *self_v, const EXP_PYATTRIBUTE_DEF *attrdef);

#endif // WITH_PYTHON
};

#endif  // __KX_ANIMATION_EVENT_H__
