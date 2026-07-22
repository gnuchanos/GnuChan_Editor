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
 * The Original Code is Copyright (C) 2022-2024 by Range Engine.
 *
 * The Original Code is: all of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

 /** \file gameengine/Ketsji/KX_DebugMode.cpp
  *  \ingroup ketsji
  */

#ifdef WITH_BULLET
#  include "LinearMath/btIDebugDraw.h"
#endif  // WITH_BULLET

#include "KX_Camera.h"
#include "KX_DebugMode.h"
#include "KX_Globals.h"
#include "KX_KetsjiEngine.h"
#include "KX_Scene.h"

#include "RAS_Query.h"

#include "EXP_ListValue.h"

#include "PHY_IPhysicsEnvironment.h"

#include "IconsForkAwesome.h"
#include "KX_Imgui_Impl_Inputs.h"
#include "imgui_impl_opengl3.h"

bool sortProfilingObjects(KX_GameObject *A, KX_GameObject *B)
{
  KX_GameObject::DebugProfilingData dA = A->GetDebugTimeProfiling();
  KX_GameObject::DebugProfilingData dB = B->GetDebugTimeProfiling();

  float totA = dA.m_time_components + dA.m_time_animation;
  float totB = dB.m_time_components + dB.m_time_animation;

  return totA > totB;
}

KX_DebugMode::KX_DebugMode()
    : m_hideDebugModePanels(false),
      m_hideDebugMode(false),
      m_autoResize(true),
      m_history(5.0f),
      m_axisCondition(ImGuiCond_Once),
      m_axisLimit(16.0f),
      m_cameraSpeed(0.3f),
      m_profileSize(1.0f),
      m_debugPropertiesSize(1.0),
      m_AdvancedProfiling(false),
      m_ObjectProfiling_PanelSize(1.0f),
      m_sortObjectProfilingDelay(0),
      m_sortObjectProfilingDelay_time(120),
      m_advprofileTime(0.0f),
      m_pickSceneObject(false),
      m_showOnlyFrameRate(false),
      m_activeGizmo(GIZMO_NONE),
      m_mouseLastPos(ImVec2(-1.0, -1.0)),
      m_mouseRoll(0.0f),
      m_objLastPos(mt::zero3),
      m_objLastOri(mt::zero3),
      m_objLastScale(mt::zero3),
      m_transform_orientation(0),
      imgui_showDebugMenuTop(true),
      imgui_showDebugPhys(false),
      imgui_showArmatures(false),
      imgui_controllActiveCamera(false),
      imgui_blockInputEvents(false),
      imgui_suspendedScene(false),
      imgui_ob_panel_expandObject(false),
      imgui_KXObSelected(nullptr),
      imgui_cameraSelectedGameObj(nullptr),
      imgui_cameraSelected(0),
      imgui_debugListProp_Index(0),
      imgui_sceneProp_Index(0)
{

};

KX_DebugMode::~KX_DebugMode()
{
};

/* Render Debug Mode */
void KX_DebugMode::RenderImguiDebugMode() {
	/* Don't draw debug mode */
	if (m_hideDebugMode) {
		return;
	}

	// Docking Viewport
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	if (imgui_showDebugMenuTop) { RenderImguiDebug_MainMenuTop(); }

	/* Select an gameobject in the scene */
	if (m_pickSceneObject) {
		/* Infor Window */
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		center.y -= 100;
		ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

		ImGui::SetNextWindowBgAlpha(0.5f); // Transparent background
		if (ImGui::Begin("PickGameObject", nullptr, window_flags)) {
			ImGui::TextUnformatted(ICON_FK_MOUSE_POINTER "Select an GameObject");
			ImGui::TextUnformatted("   (Right-click to cancel)");
		}
		ImGui::End();
	}

	// Draw object manipulation stuff.
	if (imgui_KXObSelected) {
		/* DrawOBManipulatorPanel screen position. Shared data with DrawOBManipulator_ExpandedPanel */
		ImGuiViewport *viewport = ImGui::GetMainViewport();
		const float menuBarHeight = ImGui::GetFrameHeight();

		ImVec2 windowPos = ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f,
								  viewport->Pos.y + menuBarHeight);

		/* Draw */
		const bool draw = DrawOBManipulatorPanel(imgui_KXObSelected, windowPos);
		if (draw) {
			DrawGizmo(imgui_KXObSelected);

			// Draw Expanded Game Object Options
			if (imgui_ob_panel_expandObject) {
				DrawOBManipulator_ExpandedPanel(imgui_KXObSelected, windowPos);
			}
		}
	}

	/* Don't draw debug mode panels */
	if (m_hideDebugModePanels) {
		return;
	}

	/* Windows */
	if (ImGui::Begin("Options")) {
		RenderOptionsPanel();
	}
	ImGui::End();
	if (ImGui::Begin("Profiling")) {
		RenderProfiling();
	}
	ImGui::End();
	if (ImGui::Begin("Scene")) {
		RenderScenePanel();
	}
	ImGui::End();

	// Examples
	//ImGui::ShowDemoWindow();
	//ImPlot::ShowDemoWindow();

	// Render Sub-Panel (if necessary)
	if (imgui_controllActiveCamera) {
		RenderCameraControllPanel();
	}
}

// Override the old Debug properties.
void KX_DebugMode::RenderDebugProperties()
{
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                  ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
  const int PAD = 1;
  float default_window_size_Y = 140.0f;
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImVec2 work_pos = viewport->WorkPos;  // Use work area to avoid menu-bar/task-bar, if any!
  ImVec2 window_pos, window_pos_pivot;
  window_pos.x = (work_pos.x + PAD);
  window_pos.y = (work_pos.y + PAD);
  window_pos_pivot.x = 0.0f;
  window_pos_pivot.y = 0.0f;
  ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
  ImGui::SetNextWindowViewport(viewport->ID);
  
  ImGui::SetNextWindowBgAlpha(0.35f);  // Transparent background
  if (ImGui::Begin("DebugProperties", (bool *)0, window_flags)) {
    ImGui::SetWindowFontScale(m_profileSize * 0.8f);
    
    double tottime = KX_GetActiveEngine()->m_logger.GetAverage();
    if (tottime < 1e-6) {
      tottime = 1e-6;
    }

    ImGui::TextUnformatted("Framerate");
    ImGui::SameLine(90);
    ImVec4 color(0, 255, 0, 225);  // green (Default)
    if ((1.0f / tottime) < 24) {
      color = ImVec4(225, 255, 0, 225);
    }
    else if ((1.0f / tottime) < 30) {
      color = ImVec4(225, 0, 0, 225);
    }

    ImGui::TextColored(color, "%5.2fms |", (tottime * 1000.0f));
    ImGui::SameLine();
    ImGui::TextColored(color, "(%.0fFPS)", KX_GetActiveEngine()->GetAverageFrameRate());

	// Show only framerate (no button).
	if (!KX_GetActiveEngine()->GetFlag(KX_KetsjiEngine::SHOW_PROFILE)) {
      ImGui::SetWindowSize(ImVec2(200.0f * m_profileSize, 25.0f));
      ImGui::End();
      return;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton(m_showOnlyFrameRate ? ICON_FK_PLUS : ICON_FK_MINUS)) {
      m_showOnlyFrameRate = !m_showOnlyFrameRate;
    }

	// Show Render Queries.
    if (KX_GetActiveEngine()->GetFlag(KX_KetsjiEngine::SHOW_RENDER_QUERIES)) {
      default_window_size_Y += 45.0f;
      ImGui::Separator();
      ImGui::TextUnformatted("Render Queries");

      std::string debugtxt;

      for (unsigned short i = 0; i < KX_KetsjiEngine::QUERY_MAX; ++i) {

        ImGui::TextUnformatted(KX_GetActiveEngine()->GetRenderQueryLabel(i).c_str());
		// Tooltip.
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", profileQueryTips[i].c_str());
        }
        ImGui::SameLine();
        if (i == KX_KetsjiEngine::QUERY_TIME) {
          ImGui::TextColored(ImVec4(0, 255, 0, 225), "%.2fms", (((float)KX_GetActiveEngine()->GetRenderQueryValue(i)) / 1e6));
        }
        else {
          ImGui::TextColored(ImVec4(0, 255, 0, 225), "%i", KX_GetActiveEngine()->GetRenderQueryValue(i));
        }
      }
    }
	
	// Show only framerate.
	if (m_showOnlyFrameRate) {
      ImGui::SetWindowSize(ImVec2(200.0f * m_profileSize, 25.0f * m_profileSize));
      ImGui::End();
      return;
    }

	ImGui::Separator();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    const ImU32 col = ImColor(0.2f, 0.2f, 0.2f, 0.85f);
    for (int j = KX_GetActiveEngine()->tc_first; j < KX_GetActiveEngine()->tc_numCategories; j++) {
      double time = KX_GetActiveEngine()->m_logger.GetAverage((KX_KetsjiEngine::KX_TimeCategory)j);
      int percentage = (int)(time / tottime * 100.0f);

      /* Draw Rect */
      const ImVec2 p = ImGui::GetCursorScreenPos();
      const float rectPad = 0.95f + (m_profileSize * 0.05f);
      float x = ((p.x + 138) + (-1.0f + rectPad) * 1000.0f) * rectPad;
      float y = (p.y + 1.5f) * rectPad;
      const float sz = percentage * 0.5f;
      draw_list->AddRectFilled(ImVec2(x, y), ImVec2((x + sz), (y + 8) * rectPad), col);

      /* Draw Text */
      color = ImVec4(0, 255, 0, 225);  // green (Default)
      if (j != KX_KetsjiEngine::KX_TimeCategory::tc_outside && j != KX_KetsjiEngine::KX_TimeCategory::tc_latency) {
        /* Red */
        if (percentage > 50)
          color = ImVec4(225, 0, 0, 225);
        /* Yellow */
        else if (percentage > 25)
          color = ImVec4(225, 255, 0, 225);
      }
      else {
        color = ImVec4(225, 255, 255, 225);
      }

      ImGui::TextColored(color, "%s", KX_GetActiveEngine()->m_profileLabels[j].c_str());
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", profileTips[j].c_str());
      }
      ImGui::SameLine(90);
      ImGui::Text("%5.2fms | %i%%", (time * 1000.f), percentage);
    }

    // Properties
    if (KX_GetActiveEngine()->GetFlag(KX_KetsjiEngine::SHOW_DEBUG_PROPERTIES)) {
      ImGui::Separator();
      if (ImGui::CollapsingHeader("Game Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        default_window_size_Y += 95.f * m_debugPropertiesSize;
        ImGui::SetWindowSize(ImVec2(200.0f * m_profileSize, default_window_size_Y));

        if (ImGui::BeginChild("Properties")) {
          EXP_ListValue<KX_Scene> *scenes = KX_GetActiveEngine()->GetScenes();

          for (int i = 0; i < scenes->GetCount(); i++) {
            KX_Scene *scene = scenes->GetValue(i);
            scene->RenderDebugPropertiesImGui(i);
          }
        }
        ImGui::EndChild();
      }
      else {
        ImGui::SetWindowSize(ImVec2(200.0f * m_profileSize, default_window_size_Y * m_profileSize + 10));
      }
    }
    else {
      ImGui::SetWindowSize(ImVec2(200.0f * m_profileSize, default_window_size_Y * m_profileSize));
    }

    ImGui::End();
  }
}

void KX_DebugMode::RenderScenePanel() {
	//KX_Imgui *imgui = KX_GetActiveEngine()->GetImgui();

	/* Scene Objects */
	if (ImGui::CollapsingHeader("Objects")) {
		if (ImGui::BeginChild("Scene Objects", ImVec2(0, 150), true)) {
			// Make objects scene list.
			int sceneIndex = 0;

			for (KX_Scene *scene : KX_GetActiveEngine()->GetScenes()) {
				char sceneName[64];
				// Scene Index / Name.
				sprintf(sceneName, "%s ##%i", scene->GetName().c_str(), sceneIndex);

				if (ImGui::CollapsingHeader(sceneName)) {
					for (int i = 0; i < scene->GetObjectList()->GetCount(); i++)
					{
						KX_GameObject *gameObj = scene->GetObjectList()->GetValue(i);

						if (ImGui::Selectable(gameObj->GetName().c_str(), false)) {
							imgui_KXObSelected = gameObj;
						}
					}
				}
			sceneIndex++;
			}
			ImGui::EndChild();
		}
		if (ImGui::Button("Pick Object", ImVec2(100, 0))) {
			m_pickSceneObject = !m_pickSceneObject;
		}

		ImGui::SameLine();
		if (ImGui::Button("Deselect Object", ImVec2(100, 0))) {
			imgui_KXObSelected = nullptr;
		}
	}

	if (ImGui::CollapsingHeader("Cameras")) {
		/* Camera Change */
		ImGui::TextUnformatted("Scene Cameras");
		ImGui::BulletText("Double click to control the camera");
		ImGui::BeginChild("Scene Cameras", ImVec2(0, 150), true);

		// Always Use Front Scene.
		KX_Scene *scene = KX_GetActiveEngine()->GetScenes()->GetFront();
		for (int i = 0; i < scene->GetCameraList()->GetCount(); i++)
		{
			char label[64] = "Editor Camera";
			KX_Camera *camOb = scene->GetCameraList()->GetValue(i);

			if (camOb->GetName() != "__default__cam__") {
				sprintf(label, "%s", camOb->GetName().c_str());
			}
			
			if (ImGui::Selectable(label, imgui_cameraSelected == i)) {
				// Controll the camera, if selected previously
				if (imgui_cameraSelected == i) {
					// enable camera controller
					this->EnableCameraController(camOb, false);
				}
				// If it wasn't double-clicked, let's just look at the camera
				else {
					scene->SetActiveCamera(camOb);
					imgui_cameraSelected = i;
					
					if (camOb->GetName() == "__default__cam__") {
						sprintf(label, "%s", camOb->GetName().c_str());
						imgui_blockInputEvents = true;
					}
					else {
						imgui_blockInputEvents = false;
					}
				}
			}
		}
		ImGui::EndChild();
	}

	if (ImGui::CollapsingHeader("Inactive Objects")) {
		ImGui::TextUnformatted("Inactive Layer Objects");

		ImGui::BeginChild("Add Objects", ImVec2(0, 150), true);
		for (int i = 0; i < KX_GetActiveScene()->GetInactiveList()->GetCount(); i++)
		{
			KX_GameObject* gameObj = KX_GetActiveScene()->GetInactiveList()->GetValue(i);

			ImGui::TextUnformatted(gameObj->GetName().c_str());

			ImGui::SameLine(150);

			std::string buttonName = "Add##";
			buttonName += std::to_string(i);

			if (ImGui::SmallButton(buttonName.c_str())) {
				KX_GameObject *replica = KX_GetActiveScene()->AddReplicaObject(gameObj, gameObj);
				replica->Release();
			}
		}
		ImGui::EndChild();
	}
}

void KX_DebugMode::RenderOptionsPanel() {
	ImGui::Checkbox("Show debug menu topbar", &imgui_showDebugMenuTop);

	ImGui::Separator();

	ImGui::TextUnformatted("Profile settings");
	if (ImGui::Button("Show Framerate")) {
		KX_GetActiveEngine()->ToggleFlag(KX_KetsjiEngine::SHOW_FRAMERATE);
	}
	ImGui::SameLine();
	if (ImGui::Button("Show Profile")) {
		KX_GetActiveEngine()->ToggleFlag(KX_KetsjiEngine::SHOW_PROFILE);
	}
	ImGui::SameLine();
	if (ImGui::Button("Show Debug Properties")) {
		KX_GetActiveEngine()->ToggleFlag(KX_KetsjiEngine::SHOW_DEBUG_PROPERTIES);
	}

	ImGui::SliderFloat("Profile Size", &m_profileSize, 0.9f, 1.5f);
	ImGui::SliderFloat("Debug Properties Size", &m_debugPropertiesSize, 1.0f, 10.0f);
}

void KX_DebugMode::RenderProfiling() {
	ImGui::SetWindowFontScale(0.8f);

	if (ImGui::CollapsingHeader("Graph Profiler")) {
		//ImGui::SetWindowSize(ImVec2(0, 20.0f));
		if (!KX_GetActiveScene()->m_suspend) {
			m_advprofileTime += ImGui::GetIO().DeltaTime;

			// Physics, Logic, Animations, Scenegraph, Rasterizer, Overhead 
			for (int i = 7; i >= 0; i--) {
				KX_KetsjiEngine::KX_TimeCategory tc = (KX_KetsjiEngine::KX_TimeCategory)i;
				if (i == KX_KetsjiEngine::tc_network) { tc = KX_KetsjiEngine::tc_scenegraph; }
				if (i == KX_KetsjiEngine::tc_services) { tc = KX_KetsjiEngine::tc_overhead; }

				float time = KX_GetActiveEngine()->m_logger.GetAverage(tc) * 1000.f;

				/* We need to stack all the values.
				/* I tried to do a for loop but in this case i really couldn't do it.. if someone, be careful. */
				float stime = 0;
				if (i == KX_KetsjiEngine::tc_rasterizer) {
					stime = KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_overhead) * 1000.f;
				}
				else if (i == KX_KetsjiEngine::tc_scenegraph) {
					stime = KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_overhead) * 1000.f;
					stime += KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_rasterizer) * 1000.f;
				}
				else if (i == KX_KetsjiEngine::tc_animations) {
					stime = KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_overhead) * 1000.f;
					stime += KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_rasterizer) * 1000.f;
					stime += KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_scenegraph) * 1000.f;
				}
				else if (i == KX_KetsjiEngine::tc_logic) {
					stime = KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_overhead) * 1000.f;
					stime += KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_rasterizer) * 1000.f;
					stime += KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_scenegraph) * 1000.f;
					stime += KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_animations) * 1000.f;
				}
				else if (i == KX_KetsjiEngine::tc_physics) {
					stime = KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_overhead) * 1000.f;
					stime += KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_rasterizer) * 1000.f;
					stime += KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_scenegraph) * 1000.f;
					stime += KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_animations) * 1000.f;
					stime += KX_GetActiveEngine()->m_logger.GetAverage(KX_KetsjiEngine::tc_logic) * 1000.f;
				}

				if (stime != 0) {
					time += stime;
				}

				// Debugging
				/*using namespace std;
				if (i == KX_KetsjiEngine::tc_physics) { cout << time << "\n";  }*/

				m_profileBuffer[i].AddPoint(m_advprofileTime, time);
			}
		}

		ImGui::SliderFloat("History", &m_history, 1.0f, 15.0f, "%.1f s");
		ImGui::SameLine();
		ImGui::Checkbox("AutoResize", &m_autoResize);

		if (ImPlot::BeginPlot("##MainProfile", ImVec2(0, 175))) {

			ImPlot::SetupAxes("Time (Seconds)", "Ms(milliseconds)");
			ImPlot::SetupAxisLimits(ImAxis_X1, m_advprofileTime - m_history, m_advprofileTime, ImGuiCond_Always);
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0, m_axisLimit, m_axisCondition);

			if (ImPlot::IsPlotHovered()) {
				m_axisCondition = ImGuiCond_Once;
			}
			else if (m_autoResize){
				m_axisLimit = m_profileBuffer[0].GetLastPointY().y + 5;
				m_axisCondition = ImGuiCond_Always;
			}
		
			for (int i = 0; i < 8; i++) {
				int tc = i;
				if (i == KX_KetsjiEngine::tc_network) { tc = KX_KetsjiEngine::tc_scenegraph; }
				else if (i == KX_KetsjiEngine::tc_services) { tc = KX_KetsjiEngine::tc_overhead; }

				ImPlot::PlotShaded(KX_GetActiveEngine()->m_profileLabels[tc].c_str(), &m_profileBuffer[tc].Data[0].x, &m_profileBuffer[tc].Data[0].y, m_profileBuffer[tc].Data.size(), 0, 0, m_profileBuffer[tc].Offset, 2 * sizeof(float));
			}

			double linePos = 16.0;
			ImPlot::DragLineY(0, &linePos, ImVec4(1, 1, 0, 1), 1, ImPlotDragToolFlags_NoInputs);
			ImPlot::TagY(16.f, ImVec4(1, 1, 0, 1), "60FPS");

			linePos = 33.0;
			ImPlot::DragLineY(0, &linePos, ImVec4(1, 1, 0, 1), 1, ImPlotDragToolFlags_NoInputs);
			ImPlot::TagY(33.f, ImVec4(1, 1, 0, 1), "30FPS");

			linePos = 66.0;
			ImPlot::DragLineY(0, &linePos, ImVec4(1, 1, 0, 1), 1, ImPlotDragToolFlags_NoInputs);
			ImPlot::TagY(66.f, ImVec4(1, 1, 0, 1), "15FPS");

			// FPS Line
			linePos = m_profileBuffer[0].GetLastPointY().y;
			ImPlot::DragLineY(0, &linePos, ImVec4(0, 1, 0, 0.5f), 1, ImPlotDragToolFlags_NoInputs);
			ImPlot::TagY(m_profileBuffer[0].GetLastPointY().y, ImVec4(0, 1, 0, 1), "FPS %.1f", (1.0f / KX_GetActiveEngine()->m_logger.GetAverage()));

			ImPlot::EndPlot();
		}
	}

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
          ImGui::SetTooltip("Activate Graph Profiler (WARNING: This has a high impact on performance).");
    }

	// Toggle Advanced Profiling.
	m_AdvancedProfiling = ImGui::CollapsingHeader("Advanced Profiling");
    if (m_AdvancedProfiling) {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.3f, 0.3f, 0.7f)); // Change Background color.
		if (ImGui::BeginChild("Advanced Profiling Panel")) {

			// Draw Advanced Profiling Panel.
			ImGui::SliderFloat("Object Panel Size", &m_ObjectProfiling_PanelSize, 0.1f, 2.0f, "%.1f");

			// Change Font scale.
			ImGui::SetWindowFontScale(m_ObjectProfiling_PanelSize);

			RenderObjectProfiling();

			// Restore Original Font Scale.
			ImGui::SetWindowFontScale(1.0f);

			ImGui::BulletText("Organized list of objects, major/minor performance impact");
			// List of objects, Order of more/less impact on performance.
			if (ImGui::BeginChild("Profiling Objects List", ImVec2(0, 150), true)) {

				// Update Vector List and sort.
				if (m_sortObjectProfilingDelay >= m_sortObjectProfilingDelay_time) {
					EXP_ListValue<KX_GameObject> *objects = KX_GetActiveEngine()->GetScenes()->GetFront()->GetObjectList();

					m_ObjectsProfiling.clear(); // clear

					for (KX_GameObject *gameobj : objects) {
						m_ObjectsProfiling.push_back(gameobj);
					}
					std::sort(m_ObjectsProfiling.begin(), m_ObjectsProfiling.end(), sortProfilingObjects);
					m_sortObjectProfilingDelay = 0;
				}

				// Draw list.
				int index = 0;
				for (KX_GameObject *gameObj : m_ObjectsProfiling) {

					char name[128];
					sprintf(name, "%i - %s", (index + 1), gameObj->GetName().c_str());
					if (ImGui::Selectable(name, false)) { // ToDo
						imgui_KXObSelected = gameObj;
					}
					index++;
				}

				m_sortObjectProfilingDelay++;
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
          ImGui::SetTooltip("Activate Advanced Profiling (WARNING: This has a high impact on performance).");
    }
}

void KX_DebugMode::RenderImguiDebug_MainMenuTop() {
	//KX_Imgui *imgui = KX_GetActiveEngine()->GetImgui();

	if (ImGui::BeginMainMenuBar())
	{
		ImGui::TextColored(ImVec4(0, 255, 0, 255), "Debug Mode");
		ImGui::Spacing();
		ImGui::TextUnformatted("GameEngine Options:");

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);  // No Button Round

		if (ImGui::Checkbox("Debug Physics", &imgui_showDebugPhys)) {
#ifdef WITH_BULLET
			if (imgui_showDebugPhys) {
				KX_GetActiveScene()->GetPhysicsEnvironment()->SetDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb | btIDebugDraw::DBG_DrawContactPoints |
																		   btIDebugDraw::DBG_DrawText | btIDebugDraw::DBG_DrawConstraintLimits | btIDebugDraw::DBG_DrawConstraints);
			}
			/* Disable */
			else { KX_GetActiveScene()->GetPhysicsEnvironment()->SetDebugMode(0); }
#endif // WITH_BULLET
		}
		if (ImGui::Checkbox("Show Armatures", &imgui_showArmatures)) {
			KX_GetActiveEngine()->SetShowArmatures(imgui_showArmatures == 1 ? KX_DebugOption::FORCE : KX_DebugOption::DISABLE);
		}

		// Center Align Text
		float posX = ((ImGui::GetWindowWidth() / 2) 
					 - ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);

		if (posX > ImGui::GetCursorPosX()) {
			ImGui::SetCursorPosX(posX);
		}
		
		// Exit Game Button
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(123, 0, 0, 255));
		if (ImGui::Button(ICON_FK_STOP "")) {
			KX_GetActiveEngine()->RequestExit(KX_ExitInfo::QUIT_GAME);
		}
		ImGui::PopStyleColor();

		//  Pause Scene Button
		bool paused = KX_GetActiveScene()->IsSuspended();
		if (paused) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(123, 0, 0, 255));
		}

		if (ImGui::Button(ICON_FK_PAUSE "")) {
			if (KX_GetActiveScene()->IsSuspended()) {
				for (KX_Scene *scene : KX_GetActiveEngine()->GetScenes()) {
					scene->Resume();
				}
				imgui_suspendedScene = false;
			}
			else {
				for (KX_Scene *scene : KX_GetActiveEngine()->GetScenes()) {
					scene->Suspend();
				}
				imgui_suspendedScene = true;
			}
		}

		if (paused) {
			ImGui::PopStyleColor();
		}

		/* Hacky: Extremely garbage code below, I thought it best to do a trick for the frame advance button because */
		/* I would have to make several scene update calls here, or to avoid code replication from KetsjiEngine create a - */
		/* special function for updating the whole scene at KetsjiEngine. */
		/* */
		/* The trick is simple, right after pressing the button it will Resume() the scene, */
		/* in the next frame it will go back and check if the scene was suspended to know if the scene was suspended before by debug mode, if so it suspends the scene again. */
		if (imgui_suspendedScene && !KX_GetActiveScene()->m_suspend) {
			for (KX_Scene *scene : KX_GetActiveEngine()->GetScenes()) {
				scene->Suspend();
			}
		}

		// Resume Scene Button
		if (ImGui::Button(ICON_FK_FORWARD "")) {
			for (KX_Scene *scene : KX_GetActiveEngine()->GetScenes()) {
				scene->Resume();
			}
		}

		// Camera Controller Button
		if (ImGui::Button(ICON_FK_CAMERA "")) {
			SCA_IInputDevice *inputDevice = KX_GetActiveEngine()->GetInputDevice();
			bool use_active = inputDevice->GetInput(SCA_IInputDevice::LEFTSHIFTKEY).Find(SCA_InputEvent::ACTIVE);
			this->EnableCameraController(KX_GetActiveEngine()->GetScenes()->GetFront()->GetActiveCamera(), use_active);
		}

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Take control of the dev cam from the active camera. Hold LEFTSHIFT to take control of the active camera.");
		}

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Select an object in the scene with the mouse.");
		}

		ImGui::PopStyleVar(); // Restore Button rounding.
		
		// Right Align Text
		bool allowMouse = KX_GetActiveEngine()->GetCanvas()->GetDebugModeAllowMouse();
		const std::string text = ICON_FK_MOUSE_POINTER "Press [F1] to %s mouse rights  ";
		const std::string textHideDMP = ICON_FK_GNU_SOCIAL "[F2] to hide Debug Mode Panels  ";
		const std::string textHideDM = ICON_FK_GNU_SOCIAL "[F3] to hide Debug Mode  ";
		const std::string textSelectKXObj = ICON_FK_GNU_SOCIAL "[F4] to select an object    ";

		const float textSize = (ImGui::CalcTextSize(text.c_str()).x + ImGui::CalcTextSize(textHideDMP.c_str()).x + 
								ImGui::CalcTextSize(textHideDM.c_str()).x + ImGui::CalcTextSize(textSelectKXObj.c_str()).x);
		posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - textSize
				- ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);

		if (posX > ImGui::GetCursorPosX()) {
			ImGui::SetCursorPosX(posX);
		}
		
		ImGui::TextColored(allowMouse ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
						   text.c_str(), allowMouse ? "release" : "get");

		ImGui::TextColored(m_hideDebugModePanels ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f), textHideDMP.c_str());
		
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), textHideDM.c_str());

		ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), textSelectKXObj.c_str());

		ImGui::EndMainMenuBar();
	}
}

/* Sub-Menus */
void KX_DebugMode::RenderCameraControllPanel() {
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	center.y += 150;
	ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
	ImGui::SetNextWindowBgAlpha(0.5f); // Transparent background
	if (ImGui::Begin("CameraControllPanel", nullptr, window_flags)) {
		ImGui::TextUnformatted("(left-click to stop)");
		ImGui::Text("Speed: %.2f", m_cameraSpeed);
		ImGui::Separator();
		//ImGui::Text("Potato");
	}
	ImGui::End();
	ImGui::PopStyleColor();

	/// Camera Speed
	SCA_IInputDevice *inputDevice = KX_GetActiveEngine()->GetInputDevice();
	int mouseWheelUp = inputDevice->GetInput(SCA_IInputDevice::WHEELUPMOUSE).Find(SCA_InputEvent::ACTIVE);
	int mouseWheelDown = inputDevice->GetInput(SCA_IInputDevice::WHEELDOWNMOUSE).Find(SCA_InputEvent::ACTIVE);

	if ((mouseWheelUp && m_cameraSpeed <= 9.8f) || (mouseWheelDown && m_cameraSpeed > 0.01f)) {
		int wheeldir = (mouseWheelUp - mouseWheelDown);
		float add = m_cameraSpeed < 0.2f ? 0.01f : 
		            m_cameraSpeed < 2.0f ? 0.1f : 0.5f;

		m_cameraSpeed += add * wheeldir;
	}
}

void KX_DebugMode::RenderGameObjectPanel() {
}

void KX_DebugMode::RenderObjectProfiling() {
  KX_Scene *scene = KX_GetActiveEngine()->GetScenes()->GetFront();
  EXP_ListValue<KX_GameObject> *objects = scene->GetObjectList();

  int index = 0;
  for (KX_GameObject *gameobj : objects) {
    bool isCamera = (gameobj == scene->GetActiveCamera());

	// get profiling.
    KX_GameObject::DebugProfilingData debugData = gameobj->GetDebugTimeProfiling();
    float logicTime = debugData.m_time_components * 1000.0f;
    float animTime = debugData.m_time_animation * 1000.0f;

	if (!debugData.m_render) {
		continue;
	}

	const char *profilingBase = R"(%s%s Object Name: %s
 Logic: %.4fms
 Animation: %.4fms)";

	// Format Text.
    char profilingText[128];
    sprintf(profilingText, profilingBase, 
		isCamera ? R"( Active Camera Profile
)" : "",
		ICON_FK_INFO_CIRCLE, 
		gameobj->GetName().c_str(), 
		logicTime, animTime);

	
    ImVec2 window_size = ImGui::GetWindowViewport()->WorkSize;
    ImVec2 toScreenProj = !isCamera ? GetGameObjectProjectedOnScreen(gameobj) : ImVec2(window_size.x * 0.5f, window_size.y * 0.8f);

	// Render Quad. for better view.
	ImVec2 textSize = ImGui::CalcTextSize(profilingText);
	ImVec2 rectMax = ImVec2(toScreenProj.x + textSize.x, toScreenProj.y + textSize.y);
    ImGui::GetBackgroundDrawList()->AddRectFilled(toScreenProj, rectMax, ImColor(0.2f, 0.2f, 0.2f, 0.85f));

	// Render Text.
    ImGui::GetBackgroundDrawList()->AddText(toScreenProj, IM_COL32(255, 255, 255, 255), profilingText);
    index++;
  }
}

/* Events Process */
void KX_DebugMode::ProcessDebugEvents() {
	SCA_IInputDevice *inputDevice = KX_GetActiveEngine()->GetInputDevice();

	/* Debug Mode Camera Controll */
	if (imgui_controllActiveCamera) {
		KX_Scene *scene = KX_GetActiveEngine()->GetScenes()->GetFront();
		float dt = KX_GetActiveEngine()->GetEngineDeltaTime();
		mathfu::vec3 dloc(0, 0, 0);

		/// Movement
		if (inputDevice->GetInput(SCA_IInputDevice::WKEY).Find(SCA_InputEvent::ACTIVE)) { dloc.z = -m_cameraSpeed; }
		else if (inputDevice->GetInput(SCA_IInputDevice::SKEY).Find(SCA_InputEvent::ACTIVE)) { dloc.z = m_cameraSpeed; }
		if (inputDevice->GetInput(SCA_IInputDevice::AKEY).Find(SCA_InputEvent::ACTIVE)) { dloc.x = -m_cameraSpeed; }
		else if (inputDevice->GetInput(SCA_IInputDevice::DKEY).Find(SCA_InputEvent::ACTIVE)) { dloc.x = m_cameraSpeed; }

		if (inputDevice->GetInput(SCA_IInputDevice::QKEY).Find(SCA_InputEvent::ACTIVE)) { dloc.y = -m_cameraSpeed; }
		else if (inputDevice->GetInput(SCA_IInputDevice::EKEY).Find(SCA_InputEvent::ACTIVE)) { dloc.y = m_cameraSpeed; }

		// Camera Run
		if (inputDevice->GetInput(SCA_IInputDevice::LEFTSHIFTKEY).Find(SCA_InputEvent::ACTIVE)) {
			dloc *= 2;
		}

		// ApplyMovement
		scene->GetActiveCamera()->ApplyMovement((dloc * 60) * dt, true);

		/// MouseLook
		KX_GetActiveEngine()->GetPythonMouse()->Recenter(true);
		mathfu::vec2 mouseDelta = KX_GetActiveEngine()->GetPythonMouse()->GetDeltaPosition(true);
		
		// ApplyRotation
		scene->GetActiveCamera()->ApplyRotation(mathfu::vec3(0, 0, (mouseDelta.x * 60) * dt), false);
		scene->GetActiveCamera()->ApplyRotation(mathfu::vec3((mouseDelta.y * 60) * dt, 0, 0), true);

		/// DeActivate Controll
		if (inputDevice->GetInput(SCA_IInputDevice::LEFTMOUSE).Find(SCA_InputEvent::JUSTRELEASED)) {
			imgui_controllActiveCamera = false;
			KX_GetActiveEngine()->GetCanvas()->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);
		}
	}
	if (m_pickSceneObject) {
		GetGameObjectOnTheScreen();
	}

	/* Hide DebugMode Panels */
	if (inputDevice->GetInput(SCA_IInputDevice::F2KEY).Find(SCA_InputEvent::JUSTRELEASED)) {
		m_hideDebugModePanels = !m_hideDebugModePanels; // Switch
	}

	/* Hide DebugMode */
	if (inputDevice->GetInput(SCA_IInputDevice::F3KEY).Find(SCA_InputEvent::JUSTRELEASED)) {
		m_hideDebugMode = !m_hideDebugMode; // Switch
	}

	/* Select Game Object */
	if (inputDevice->GetInput(SCA_IInputDevice::F4KEY).Find(SCA_InputEvent::JUSTRELEASED)) {
		m_pickSceneObject = !m_pickSceneObject; // Switch
		imgui_KXObSelected = nullptr;
	}

	/* DebugMode Gizmos */
	if (m_activeGizmo != GIZMO_NONE) {
		if (imgui_KXObSelected) {
			KX_GameObject *gameObj = imgui_KXObSelected;
			ImGuiIO &io = ImGui::GetIO();
			// XXX I'ts based on pixels, not correct.
			ImVec2 mouseDelta = ImVec2((io.MousePos.x - m_mouseLastPos.x) * 0.02f,
									   (io.MousePos.y - m_mouseLastPos.y) * 0.02f);

			mt::vec3 worldPos = m_objLastPos;
			mt::vec3 worldScale = m_objLastScale;
			const mt::vec3 axisDirections[3] = {
				gameObj->NodeGetWorldOrientation().GetColumn(0),
				gameObj->NodeGetWorldOrientation().GetColumn(1),
				gameObj->NodeGetWorldOrientation().GetColumn(2)
			};

			/* Axis Movement */
			/* Note: there is a small problem here, it cannot check the correct direction of movement if the axis is aligned with the camera */
			/* I think this is normal behavior, need to hide the aligned axis so as not to use it ... */
			bool doMovement = true;
			if (m_activeGizmo >= ActiveGizmo::AXIS_X && m_activeGizmo <= ActiveGizmo::AXIS_Z) {
				const int axisIndex = m_activeGizmo - ActiveGizmo::AXIS_X;
				mt::vec3 axisDir = axisDirections[axisIndex];

				// Project the axis to the camera view, fix inverted axis direction.
				mt::mat3 objOrientation = gameObj->NodeGetWorldOrientation();
				mt::vec3 projectedAxis = objOrientation * axisDir;

				// Check if the camera is inverted with the view axis
				float dotProduct = projectedAxis.DotProduct(projectedAxis, m_transf_cameraDir);
				float transf_dir = (dotProduct < 0.0f) ? -1.0f : 1.0f; // Transform Direction

				mt::vec3 movement = axisDir * (transf_dir * (axisIndex == 2 ? -mouseDelta.y : mouseDelta.x));

				// Scaling Axis
				if (inputDevice->GetInput(SCA_IInputDevice::LEFTSHIFTKEY).Find(SCA_InputEvent::ACTIVE)) {
					//m_activeGizmo = ActiveGizmo::ROTATE_X;
					

					if (inputDevice->GetInput(SCA_IInputDevice::LEFTCTRLKEY).Find(SCA_InputEvent::ACTIVE)) {
						worldScale.x += movement[axisIndex];
						worldScale.y += movement[axisIndex];
						worldScale.z += movement[axisIndex];
					}
					else
						worldScale += movement;
					gameObj->NodeSetWorldScale(worldScale);
					doMovement = false; // Scaling, cancel transform
				}
				else {
					// Normal Transform
					if (!m_transform_orientation) {
						worldPos[axisIndex] += movement[axisIndex]; // Local Transform
					} else {
						worldPos += movement;
					}
				}
			}

			/* Center Axis Movment */
			if (m_activeGizmo == ActiveGizmo::AXIS_CENTER) {
				KX_Camera *camera = gameObj->GetScene()->GetActiveCamera();
				mt::mat3 orientation = camera->NodeGetWorldOrientation();

				int mouseWheelUp = inputDevice->GetInput(SCA_IInputDevice::WHEELUPMOUSE).Find(SCA_InputEvent::ACTIVE);
				int mouseWheelDown = inputDevice->GetInput(SCA_IInputDevice::WHEELDOWNMOUSE).Find(SCA_InputEvent::ACTIVE);

				if (mouseWheelUp || mouseWheelDown) {
					m_mouseRoll += (mouseWheelDown - mouseWheelUp) * 0.5f;
				}

				mt::vec3 deltaMove = (orientation.GetColumn(0) * mouseDelta.x + orientation.GetColumn(1) * -mouseDelta.y + // Plane grab method
									  orientation.GetColumn(2) * m_mouseRoll); // Camera deep movement method
				worldPos += deltaMove;
			}

			/* Rotation Gizmo */
			else if (m_activeGizmo >= ActiveGizmo::ROTATE_GLOBAL && m_activeGizmo <= ActiveGizmo::ROTATE_Z) {
				const float rotationSpeed = 0.25f;
				mt::mat3 rotationX = mt::zero3, rotationY = mt::zero3;

				// Fixed Rotation Axis
				if (m_activeGizmo >= ActiveGizmo::ROTATE_GLOBAL && m_activeGizmo <= ActiveGizmo::ROTATE_Z) {
					if (inputDevice->GetInput(SCA_IInputDevice::XKEY).Find(SCA_InputEvent::JUSTRELEASED)) {
						m_activeGizmo = ActiveGizmo::ROTATE_X;
					}
					else if (inputDevice->GetInput(SCA_IInputDevice::YKEY).Find(SCA_InputEvent::JUSTRELEASED)) {
						m_activeGizmo = ActiveGizmo::ROTATE_Y;
					}
					else if (inputDevice->GetInput(SCA_IInputDevice::ZKEY).Find(SCA_InputEvent::JUSTRELEASED)) {
						m_activeGizmo = ActiveGizmo::ROTATE_Z;
					}
				}
				
				if (m_activeGizmo == ActiveGizmo::ROTATE_GLOBAL || m_activeGizmo == ActiveGizmo::ROTATE_X)
					rotationX = mt::mat3::RotationX(-mouseDelta.y * rotationSpeed);
				if (m_activeGizmo == ActiveGizmo::ROTATE_GLOBAL || m_activeGizmo == ActiveGizmo::ROTATE_Y)
					rotationY = mt::mat3::RotationY(-mouseDelta.x * rotationSpeed);
				else if (m_activeGizmo == ActiveGizmo::ROTATE_Z)
					rotationY = mt::mat3::RotationZ(-mouseDelta.x * rotationSpeed);

				mt::mat3 newOrientation = rotationY * rotationX * m_objLastOri;
				gameObj->NodeSetGlobalOrientation(newOrientation);
			}

			if (doMovement && mouseDelta.x != 0 && mouseDelta.y != 0) {
				gameObj->NodeSetWorldPosition(worldPos);
				if (gameObj->IsDynamic())
					gameObj->SetLinearVelocity(mt::vec3(0.0f, 0.0f, 0.0f), 0);
				gameObj->NodeUpdate();
			}
		}
	}
}

ImVec2 KX_DebugMode::GetGameObjectProjectedOnScreen(KX_GameObject *gameObj)
{
  mathfu::vec3 gameObjScreenSpace = mt::vec3(gameObj->NodeGetWorldPosition());
  gameObjScreenSpace = gameObj->GetScene()->GetActiveCamera()->GetObjectProjectedOnScreenSpace(
      gameObjScreenSpace);

  ImVec2 window_size = ImGui::GetWindowViewport()->WorkSize;
  ImVec2 ObjectScreen = ImVec2(gameObjScreenSpace.x, gameObjScreenSpace.y);

  // Convert game object screen space to ImGui screen space
  return ImVec2(ObjectScreen.x * window_size.x, ObjectScreen.y * window_size.y);
}

/* Gizmo Drawing Stuff */
static ImVec2 ProjectToScreen(KX_Scene *scene, const mt::vec3 &worldPos)
{
  ImVec2 window_size = ImGui::GetWindowViewport()->WorkSize;
  mt::vec3 screenSpace = scene->GetActiveCamera()->GetObjectProjectedOnScreenSpace(worldPos);

  return ImVec2(screenSpace.x * window_size.x, screenSpace.y * window_size.y);
}

static float DistancePointToLine(ImVec2 point, ImVec2 lineStart, ImVec2 lineEnd)
{
  float A = point.x - lineStart.x;
  float B = point.y - lineStart.y;
  float C = lineEnd.x - lineStart.x;
  float D = lineEnd.y - lineStart.y;

  float dot = A * C + B * D;
  float len_sq = C * C + D * D;
  float param = (len_sq != 0) ? (dot / len_sq) : -1;

  float xx, yy;

  if (param < 0) {
    xx = lineStart.x;
    yy = lineStart.y;
  }
  else if (param > 1) {
    xx = lineEnd.x;
    yy = lineEnd.y;
  }
  else {
    xx = lineStart.x + param * C;
    yy = lineStart.y + param * D;
  }

  float dx = point.x - xx;
  float dy = point.y - yy;
  return sqrtf(dx * dx + dy * dy);
}

static bool IsMouseHoveringAxis(ImVec2 lineStart, ImVec2 lineEnd, float thickness)
{
  ImGuiIO &io = ImGui::GetIO();
  ImVec2 mousePos = io.MousePos;

  float dist = DistancePointToLine(mousePos, lineStart, lineEnd);
  return dist <= thickness;
}

static bool IsMouseHoveringCircle(ImVec2 center, float radius, float thickness)
{
  ImGuiIO &io = ImGui::GetIO();
  ImVec2 mousePos = io.MousePos;

  float dist = sqrtf((mousePos.x - center.x) * (mousePos.x - center.x) +
                     (mousePos.y - center.y) * (mousePos.y - center.y));
  return (dist >= radius - thickness && dist <= radius + thickness);
}

static void DrawGizmoAxis(KX_Scene *scene, ImDrawList *drawList, ImVec2 startPos, ImVec2 endPos, const mt::vec3 axisEnd, const mt::vec3 axisDir, ImColor color, float lineWidth, float axisLength, uint axis)
{
	// Draw Simple Axis Line
	drawList->AddLine(startPos, endPos, color, lineWidth);

	const float arrowSize = 0.15f * axisLength;
	const float arrowWidth = 0.05f * axisLength;

	/* Draw Axis Triangle Pointer .. wip														  */
	/* The drawing method is quite basic, draw the four faces of a triangle to fit together well. */
	
	/* Triangle One */
	// Calculate the base of a triangle perpendicular to the axis.
	mt::vec3 sideCross = (axis == 2) ? mt::vec3(0, 1, 0) : mt::vec3(0, 0, 1);
	mt::vec3 side1 = mt::cross(axisDir, sideCross).Normalized() * arrowWidth;
	mt::vec3 side2 = mt::cross(axisDir, side1).Normalized() * arrowWidth;

	// Define the three points of the triangle.
	mt::vec3 tip = axisEnd + axisDir * arrowSize;
	mt::vec3 base1 = axisEnd - side1;
	mt::vec3 base2 = axisEnd - side2;

	// Project the points to the screen.
	ImVec2 tipScreen = ProjectToScreen(scene, tip);
	ImVec2 base1Screen = ProjectToScreen(scene, base1);
	ImVec2 base2Screen = ProjectToScreen(scene, base2);

	// Draw
	drawList->AddTriangleFilled(base1Screen, base2Screen, tipScreen, color);

	/* Triangle Two */
	sideCross = (axis == 2) ? mt::vec3(0, -1, 0) : mt::vec3(0, 0, -1);
	side1 = mt::cross(axisDir, sideCross).Normalized() * arrowWidth;
	side2 = mt::cross(axisDir, side1).Normalized() * arrowWidth;

	tip = axisEnd + axisDir * arrowSize;
	base1 = axisEnd - side1;
	base2 = axisEnd - side2;

	tipScreen = ProjectToScreen(scene, tip);
	base1Screen = ProjectToScreen(scene, base1);
	base2Screen = ProjectToScreen(scene, base2);

	// Draw
	drawList->AddTriangleFilled(base1Screen, base2Screen, tipScreen, color);

	/* Triangle Three */
	sideCross = (axis == 0) ? mt::vec3(0, 1, 0) : mt::vec3(1, 0, 0);
	side1 = mt::cross(axisDir, sideCross).Normalized() * arrowWidth;
	side2 = mt::cross(axisDir, side1).Normalized() * arrowWidth;

	tip = axisEnd + axisDir * arrowSize;
	base1 = axisEnd - side1;
	base2 = axisEnd - side2;

	tipScreen = ProjectToScreen(scene, tip);
	base1Screen = ProjectToScreen(scene, base1);
	base2Screen = ProjectToScreen(scene, base2);

	// Draw
	drawList->AddTriangleFilled(base1Screen, base2Screen, tipScreen, color);

	/* Triangle Four (Last one) */
	sideCross = (axis == 0) ? mt::vec3(0, -1, 0) : mt::vec3(-1, 0, 0);
	side1 = mt::cross(axisDir, sideCross).Normalized() * arrowWidth;
	side2 = mt::cross(axisDir, side1).Normalized() * arrowWidth;

	tip = axisEnd + axisDir * arrowSize;
	base1 = axisEnd - side1;
	base2 = axisEnd - side2;

	tipScreen = ProjectToScreen(scene, tip);
	base1Screen = ProjectToScreen(scene, base1);
	base2Screen = ProjectToScreen(scene, base2);

	// Draw
	drawList->AddTriangleFilled(base1Screen, base2Screen, tipScreen, color);
}

static void DrawAxisColorsForDragFloat3()
{
  ImDrawList *draw_list = ImGui::GetForegroundDrawList();
  ImVec2 cursor_pos = ImGui::GetCursorScreenPos();

  const float line_width = 3.0f;
  const float line_height = ImGui::GetFrameHeight() - 2.0f;

  // Axis Color
  const ImColor col_x = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
  const ImColor col_y = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
  const ImColor col_z = ImVec4(0.3f, 0.3f, 1.0f, 1.0f);

  // Size of each parameter
  float item_spacing = ImGui::GetStyle().ItemSpacing.x;
  float field_width = (ImGui::CalcItemWidth() - (2 * item_spacing)) / 3.0f;

  ImVec2 line_start = ImVec2(cursor_pos.x, cursor_pos.y + 1);
  ImVec2 line_end = ImVec2(line_start.x + line_width, line_start.y + line_height);

  // X
  draw_list->AddRectFilled(line_start, line_end, col_x);

  // Y
  line_start.x += field_width + item_spacing;
  line_end.x += field_width + item_spacing;
  draw_list->AddRectFilled(line_start, line_end, col_y);

  // Z
  line_start.x += field_width + item_spacing - 1;
  line_end.x += field_width + item_spacing - 1;
  draw_list->AddRectFilled(line_start, line_end, col_z);
}

bool KX_DebugMode::DrawOBManipulatorPanel(KX_GameObject *gameObj, ImVec2 windowPos)
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;

	/* Gizmo Window */
	ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.0f));

	ImGui::SetNextWindowBgAlpha(0.5f); // Transparent background

	if (ImGui::Begin("GizmoPanel", nullptr, window_flags)) {
		char name[128];
		sprintf(name, "Name: %s", gameObj->GetName().c_str());
		ImGui::Text(name);
		ImVec2 nameSize = ImGui::CalcTextSize(name);
		int offset = nameSize.x > 110 ? nameSize.x : 0;

		//ImGui::Text("Gizmo Orientation:");
		const bool isGlobal = (m_transform_orientation == false);
		//ImGui::SameLine(isGlobal ? 164 : 168);
		ImGui::SameLine(110 + ((offset > 0) ? offset - 100 : 0));
		const char *orientation = isGlobal ? ICON_FK_ARROWS " Global" : ICON_FK_ARROWS " Local";
		if (ImGui::Button(orientation)) {
			m_transform_orientation = !m_transform_orientation;
		};

		ImGui::SameLine(217 + ((offset > 130) ? offset - 150 : 0));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
		if (ImGui::Button("Delete")) {
			// Delete the KX_GameObject
			gameObj->GetScene()->DelayedRemoveObject(gameObj);
			imgui_KXObSelected = nullptr;

			ImGui::PopStyleColor();
			ImGui::End();

			return false;
		}
		ImGui::PopStyleColor();

		ImGui::SameLine(255 + ((offset > 130) ? offset - 150 : 0));
		if (ImGui::Button("X", ImVec2(18, 18))) {
			imgui_KXObSelected = nullptr;
			ImGui::End();

			return false;
		}

		ImGui::SameLine(275 + ((offset > 130) ? offset - 150 : 0));
		// Expand Options
		if (ImGui::Button((imgui_ob_panel_expandObject == true) ? ICON_FK_MINUS_CIRCLE : ICON_FK_PLUS_CIRCLE "###ExpandObjectPanel")) {
			imgui_ob_panel_expandObject = !imgui_ob_panel_expandObject;
		}

		//ImGui::TextUnformatted("Transforms:");
		/* Position */
		float worldPos[3] = { gameObj->NodeGetWorldPosition().x,
							  gameObj->NodeGetWorldPosition().y,
							  gameObj->NodeGetWorldPosition().z
		};
		DrawAxisColorsForDragFloat3();
		if (ImGui::DragFloat3("###Position", worldPos, 0.1f)) {
			gameObj->NodeSetWorldPosition(mathfu::vec3(worldPos[0], worldPos[1], worldPos[2]));
			gameObj->NodeUpdate();
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FK_REFRESH "###ResetPosition")) {
			gameObj->NodeSetWorldPosition(mathfu::vec3(0, 0, 0));
			gameObj->NodeUpdate();
		}
		ImGui::SameLine();
		ImGui::TextUnformatted("Position");

		/* Rotation */
		mathfu::vec3 rotEuler = gameObj->NodeGetWorldOrientation().GetEuler();
		float worldRot[3] = { rotEuler.x,
							  rotEuler.y,
							  rotEuler.z
		};

		DrawAxisColorsForDragFloat3();
		if (ImGui::DragFloat3("###Rotation", worldRot, 0.1f)) {
			gameObj->NodeSetGlobalOrientation(mathfu::vec3(worldRot[0], worldRot[1], worldRot[2]));
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FK_REFRESH "###ResetRotation")) {
			gameObj->NodeSetGlobalOrientation(mathfu::vec3(0, 0, 0));
		}
		ImGui::SameLine();
		ImGui::TextUnformatted("Rotation");

		/* Scaling */
		float worldScaling[3] = { gameObj->NodeGetWorldScaling().x,
								  gameObj->NodeGetWorldScaling().y,
								  gameObj->NodeGetWorldScaling().z
		};

		DrawAxisColorsForDragFloat3();
		if (ImGui::DragFloat3("###Scaling", worldScaling, 0.1f)) {
			gameObj->NodeSetWorldScale(mathfu::vec3(worldScaling[0], worldScaling[1], worldScaling[2]));
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FK_REFRESH "###ResetScaling")) {
			gameObj->NodeSetWorldScale(mathfu::vec3(1.0f, 1.0f, 1.0f));
		}
		ImGui::SameLine();
		ImGui::TextUnformatted("Scaling");
	}

	ImGui::End();

	return true;
}

#define checkDrawGizmo(axis) (m_activeGizmo == ActiveGizmo::GIZMO_NONE || m_activeGizmo == (axis))
void KX_DebugMode::DrawGizmo(KX_GameObject *gameObj)
{
  KX_Scene *scene = gameObj->GetScene();
  KX_Camera *camera = scene->GetActiveCamera();

  ImVec2 screenPos2D = GetGameObjectProjectedOnScreen(gameObj);
  ImDrawList *drawList = ImGui::GetBackgroundDrawList();

  // Colors
  //const ImColor selectedColor = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
  ImVec4 xColor = ImVec4(0.8f, 0.0f, 0.0f, 1.0f);
  ImVec4 yColor = ImVec4(0.0f, 0.8f, 0.0f, 1.0f);
  ImVec4 zColor = ImVec4(0.0f, 0.0f, 0.8f, 1.0f);
  ImVec4 centerColor = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
  ImVec4 rotColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

  // Extract the direction vectors of the object's local axes in world space
  mt::vec3 xAxis, yAxis, zAxis;
  if (!m_transform_orientation) {
    xAxis = mt::vec3(1, 0, 0);
    yAxis = mt::vec3(0, 1, 0);
    zAxis = mt::vec3(0, 0, 1);
  }
  else {
    mt::mat3x4 worldTrans = gameObj->NodeGetWorldTransform();
    xAxis = worldTrans.GetColumn(0);
    yAxis = worldTrans.GetColumn(1);
    zAxis = worldTrans.GetColumn(2);
  }

  mt::vec3 objPos = gameObj->NodeGetWorldPosition();

  const float distanceToCamera = (camera->NodeGetWorldPosition() - objPos).Length();
  //const float circleScale = 350.0f / distanceToCamera;  // based on camera distance for constante scale
  
  const float axisLength = 0.15f * distanceToCamera;
  const float circleScale = (350.0f / distanceToCamera) * axisLength;

  // Calculates the final positions of each axis in the world
  mt::vec3 xEnd = objPos + xAxis * axisLength;
  mt::vec3 yEnd = objPos + yAxis * axisLength;
  mt::vec3 zEnd = objPos + zAxis * axisLength;

  // Project axis endpoints to the screen
  ImVec2 xAxisEnd = ProjectToScreen(scene, xEnd);
  ImVec2 yAxisEnd = ProjectToScreen(scene, yEnd);
  ImVec2 zAxisEnd = ProjectToScreen(scene, zEnd);

  /* Before we draw, handle the first part of interaction here */
  ActiveGizmo activeGizmo = GIZMO_NONE;

  // X Axis
  if (IsMouseHoveringAxis(screenPos2D, xAxisEnd, 6.0f)) {
    activeGizmo = ActiveGizmo::AXIS_X;
    xColor = ImVec4(xColor.x * 1.5f, xColor.y * 1.5f, xColor.z * 1.5f, 1.0f);
  }
  // Y Axis
  else if (IsMouseHoveringAxis(screenPos2D, yAxisEnd, 6.0f)) {
    activeGizmo = ActiveGizmo::AXIS_Y;
    yColor = ImVec4(yColor.x * 1.5f, yColor.y * 1.5f, yColor.z * 1.5f, 1.0f);
  }
  // Z Axis
  else if (IsMouseHoveringAxis(screenPos2D, zAxisEnd, 6.0f)) {
    activeGizmo = ActiveGizmo::AXIS_Z;
    zColor = ImVec4(zColor.x * 1.5f, zColor.y * 1.5f, zColor.z * 1.5f, 1.0f);
  }

  // Center Axis (XY mouse position based on screen)
  if (IsMouseHoveringAxis(screenPos2D, screenPos2D, 6.0f)) {
    activeGizmo = ActiveGizmo::AXIS_CENTER;
    centerColor = ImVec4(centerColor.x * 1.5f, centerColor.y * 1.5f, centerColor.z * 1.5f, 1.0f);
  }

  // Rotation (XY mouse position based on screen)
  if (IsMouseHoveringCircle(screenPos2D, circleScale, 6.0f)) {
    activeGizmo = ActiveGizmo::ROTATE_GLOBAL;
    rotColor = ImVec4(rotColor.x * 1.5f, rotColor.y * 1.5f, rotColor.z * 1.5f, 1.0f);
  }

  // Draw Axis
  const float lineWidth = 2.0f;
  if (checkDrawGizmo(ActiveGizmo::AXIS_X))
    DrawGizmoAxis(scene, drawList, screenPos2D, xAxisEnd, xEnd, xAxis, xColor, lineWidth, axisLength, 0);
  if (checkDrawGizmo(ActiveGizmo::AXIS_Y))
	DrawGizmoAxis(scene, drawList, screenPos2D, yAxisEnd, yEnd, yAxis, yColor, lineWidth, axisLength, 1);
  if (checkDrawGizmo(ActiveGizmo::AXIS_Z))
	DrawGizmoAxis(scene, drawList, screenPos2D, zAxisEnd, zEnd, zAxis, zColor, lineWidth, axisLength, 2);

  // Draw Center Axis
  if (checkDrawGizmo(ActiveGizmo::AXIS_CENTER))
	drawList->AddCircleFilled(screenPos2D, 4, (ImColor)centerColor, 24);
  // Draw Rotation Circle
  if (checkDrawGizmo(ActiveGizmo::ROTATE_GLOBAL))
    drawList->AddCircle(screenPos2D, circleScale, (ImColor)rotColor, 24, 4.0f);

  /* Handle Gizmo Interaction */
  ImGuiIO &io = ImGui::GetIO();

  // Check mouse click
  if (io.MouseDown[0]) {
    if (m_activeGizmo == ActiveGizmo::GIZMO_NONE) {
      if (m_mouseLastPos.x == -1) {
        m_mouseLastPos = io.MousePos;
        m_objLastPos = gameObj->NodeGetLocalPosition();
        m_objLastOri = gameObj->NodeGetWorldOrientation();
        m_objLastScale = gameObj->NodeGetWorldScaling();

        // get camera direction, for transformations in ProcessDebugEvents.
        m_transf_cameraDir = (camera->NodeGetWorldPosition() - gameObj->NodeGetWorldPosition()).Normalized();
      }

	  // only set active gizmo here
	  m_activeGizmo = activeGizmo;
    }
  }
  else {
    m_mouseLastPos.x = -1;
    m_mouseRoll = 0;
    m_activeGizmo = ActiveGizmo::GIZMO_NONE;
  }
}
#undef checkDrawGizmo
/* End Gizmo Drawing Stuff */

void KX_DebugMode::DrawOBManipulator_ExpandedPanel(KX_GameObject *gameObj, ImVec2 OBMan_PanelPos) {
	/* Gizmo Window */
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
	
	ImGuiViewport *viewport = ImGui::GetMainViewport();

	// XXX Duplicated code, name size
	char name[128];
	sprintf(name, "Name: %s", gameObj->GetName().c_str());
	const ImVec2 nameSize = ImGui::CalcTextSize(name);
	
	ImVec2 windowPos = ImVec2((nameSize.x > 110) ? OBMan_PanelPos.x + nameSize.x + 70 : (viewport->Pos.x + viewport->Size.x * 0.5f) + 256,
							  OBMan_PanelPos.y);
	ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(0.5f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.5f);

	if (ImGui::Begin("ExpandedOBPanel", nullptr, window_flags)) {
		mt::vec4 color = gameObj->GetObjectColor();
		float obColor[4] = {color.x, color.y, color.z, color.w};

		ImGui::Text("Object Color:");

		ImGuiColorEditFlags colorEditFlags = {ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar};
		if (ImGui::ColorEdit4("###ObColor", obColor, colorEditFlags)) {
			gameObj->SetObjectColor(mt::vec4(obColor[0], obColor[1], obColor[2], obColor[3]));
		}
		ImGui::End();
	}
	
}

void KX_DebugMode::GetGameObjectOnTheScreen() {
	SCA_IInputDevice *inputDevice = KX_GetActiveEngine()->GetInputDevice();
	bool mouseLeftPressed = inputDevice->GetInput(SCA_IInputDevice::LEFTMOUSE).Find(SCA_InputEvent::JUSTRELEASED);
	bool mouseRightPressed = inputDevice->GetInput(SCA_IInputDevice::RIGHTMOUSE).Find(SCA_InputEvent::JUSTRELEASED);

	if (mouseLeftPressed) {

		// Mouse Position Normalized
		float x_coord, y_coord;
		x_coord = KX_GetActiveEngine()->GetCanvas()->GetMouseNormalizedX(inputDevice->GetInput(SCA_IInputDevice::MOUSEX).m_values[0]);
		y_coord = KX_GetActiveEngine()->GetCanvas()->GetMouseNormalizedY(inputDevice->GetInput(SCA_IInputDevice::MOUSEY).m_values[0]);

		// Raycast to Front Scene.
		KX_GameObject::RayCastData rayData = KX_GetActiveEngine()->GetScenes()->GetFront()->GetActiveCamera()->GetScreenRayCast(
        x_coord, y_coord, 100, nullptr);
		if (rayData.m_hitObject) {
			imgui_KXObSelected = rayData.m_hitObject;
			m_pickSceneObject = false;
		}
	}

	if (mouseRightPressed) {
		imgui_KXObSelected = nullptr;
		m_pickSceneObject = false;
	}
}

void KX_DebugMode::EnableCameraController(KX_Camera *camera, bool use_active)
{
  KX_Scene *scene = KX_GetActiveEngine()->GetScenes()->GetFront();

  imgui_controllActiveCamera = true;
  imgui_blockInputEvents = true;

  if (!use_active) {
	  /* Here we will check if the selected camera is the devCam, if it is not,         */
	  /* we will create a new camera and place it in the position of the active camera, */
	  /* then we will make it the active camera and control it.                         */
	  KX_Camera *devCam = scene->GetCameraList()->FindValue("__default__cam__");

	  if (!devCam) {
		KX_GetActiveEngine()->CreateTemporaryCamera(scene, false);
		devCam = scene->GetCameraList()->FindValue("__default__cam__");
	  }
	  if (camera) {
		devCam->SetLens(camera->GetLens());
		devCam->NodeSetWorldPosition(camera->NodeGetWorldPosition());
		devCam->NodeSetGlobalOrientation(camera->NodeGetWorldOrientation());
	  }
	  scene->SetActiveCamera(devCam);
  }

  imgui_cameraSelected = (scene->GetCameraList()->GetCount() - 1);

  KX_GetActiveEngine()->GetCanvas()->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);
  KX_GetActiveEngine()->GetPythonMouse()->Recenter(true);
}

void KX_DebugMode::SaveDebugMode_Values() {
	KX_Imgui *imgui = KX_GetActiveEngine()->GetImgui();

	imgui->OpenImgui_Config_ToSave("KX_DebugMode");

	imgui->WriteBoolValue("m_hideDebugMode", m_hideDebugMode);
	imgui->WriteBoolValue("m_autoResize", m_autoResize);
	imgui->WriteFloatValue("m_history", m_history);
	imgui->WriteIntValue("m_axisCondition", (int)m_axisCondition);
	imgui->WriteFloatValue("m_axisLimit", m_axisLimit);
	imgui->WriteFloatValue("m_cameraSpeed", m_cameraSpeed);
	imgui->WriteFloatValue("m_profileSize", m_profileSize);
	imgui->WriteFloatValue("m_debugPropertiesSize", m_debugPropertiesSize);
	imgui->WriteBoolValue("m_showOnlyFrameRate", m_showOnlyFrameRate);
	imgui->WriteBoolValue("m_hideDebugModePanels", m_hideDebugModePanels);

	imgui->SaveAndCloseImgui_Config();
}

void KX_DebugMode::LoadDebugMode_Values() {
  KX_Imgui *imgui = KX_GetActiveEngine()->GetImgui();

  imgui->OpenImgui_Config_ToLoad();

  bool found = imgui->LoadSection_Config("KX_DebugMode");

  if (!found) {
    return;
  }

  // Load vars ... Keep in sync with SaveDebugMode_Values!!
  m_hideDebugMode = imgui->LoadBoolValue();
  m_autoResize = imgui->LoadBoolValue();
  m_history = imgui->LoadFloatValue();
  m_axisCondition = imgui->LoadIntValue();
  m_axisLimit = imgui->LoadFloatValue();
  m_cameraSpeed = imgui->LoadFloatValue();
  m_profileSize = imgui->LoadFloatValue();
  m_debugPropertiesSize = imgui->LoadFloatValue();
  m_showOnlyFrameRate = imgui->LoadBoolValue();
  m_hideDebugModePanels = imgui->LoadBoolValue();

  imgui->CloseImgui_Config();
}
