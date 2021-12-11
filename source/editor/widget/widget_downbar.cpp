#include "widget_downbar.h"
#include "../../imgui/imgui.h"
#include "../../engine/core/timer.h"
#include "../../engine/vk/vk_rhi.h"
#include <iomanip>
#include <sstream>

using namespace engine;

WidgetDownbar::WidgetDownbar(engine::Ref<engine::Engine> engine)
	: Widget(engine)
{
	m_title = "DownBar";
}

using TimePoint = std::chrono::system_clock::time_point;
inline std::string serializeTimePoint( const TimePoint& time, const std::string& format)
{
	std::time_t tt = std::chrono::system_clock::to_time_t(time);
	std::tm tm = *std::localtime(&tt); 
	std::stringstream ss;
	ss << std::put_time( &tm, format.c_str() );
	return ss.str();
}

void WidgetDownbar::onVisibleTick(size_t)
{
	bool hide = true;

	static ImGuiWindowFlags flag =
		ImGuiWindowFlags_NoCollapse         |
		ImGuiWindowFlags_NoResize           |
		ImGuiWindowFlags_NoMove             |
		ImGuiWindowFlags_NoScrollbar        |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoMouseInputs |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavInputs|
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_UnsavedDocument|
		ImGuiWindowFlags_NoTitleBar;

	if (!ImGui::Begin(m_title.c_str(), &hide,flag))
	{
		ImGui::End();
		return;
	}
	float fps = glm::clamp(g_timer.getCurrentSmoothFps(),0.0f,9999.0f);
	std::stringstream ss;
	ss <<std::setw(4) << std::left
		<< std::setfill(' ')<< std::fixed << std::setprecision(0) << fps;
	std::string text = u8"Rendering: " + ss.str() + "FPS";

	float vramUsage = VulkanRHI::get()->getVramUsage();
	std::stringstream ssv;
	ssv << u8"Vram: ";
	ssv <<std::setw(4) << std::left
		<< std::setfill(' ') << std::fixed << std::setprecision(2) << vramUsage * 100.0f;
	ssv << "%";
	std::string v_text = ssv.str().c_str();

	if(ImGui::BeginDownBar())
	{
		TimePoint input = std::chrono::system_clock::now();
		std::string name = serializeTimePoint(input,"%Y/%m/%d %H:%M:%S");
		ImGui::Text("@FlowerEngine ");
		ImGui::Text(name.c_str());

		constexpr float lerpMax = 120.0f;
		constexpr float lerpMin = 30.0f;

		float lerpFps = (glm::clamp(fps,lerpMin,lerpMax) - lerpMin )/ (lerpMax - lerpMin);

		glm::vec4 goodColor = {0.0f, 1.0f, 0.0f, 1.0f};
		glm::vec4 badColor = {1.0f,0.0f, 0.0f,1.0f};

		glm::vec4 lerpColor = glm::lerp(badColor,goodColor,lerpFps);
		glm::vec4 lerpColorVram = glm::lerp(badColor,goodColor,1 - vramUsage);

		ImVec4 colf;
		colf.x = lerpColor.x;
		colf.y = lerpColor.y;
		colf.z = lerpColor.z;
		colf.w = lerpColor.w;

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImVec2 p = ImGui::GetCursorScreenPos();
		const ImU32 col = ImColor(colf);

		float textStartPositionX = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(text.c_str()).x 
			- ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x;
		float sz = ImGui::GetTextLineHeight() * 0.40f;
		float fpsPointX = textStartPositionX - sz * 1.8f;

		float textStartPositionXVram = fpsPointX - ImGui::CalcTextSize((v_text + "    ").c_str()).x;
		ImGui::SetCursorPosX(textStartPositionXVram);
		ImGui::Text("%s", v_text.c_str());
		
		float x = fpsPointX;
		float y = p.y + ImGui::GetFrameHeight() * 0.51f;
		draw_list->AddCircleFilled(ImVec2(x , y ), sz, col, 40);

		colf.x = lerpColorVram.x;
		colf.y = lerpColorVram.y;
		colf.z = lerpColorVram.z;
		colf.w = lerpColorVram.w;
		const ImU32 colx = ImColor(colf);
		x = textStartPositionXVram - sz * 1.8f;
		draw_list->AddCircleFilled(ImVec2(x , y ), sz, colx, 40);
		

		ImGui::SetCursorPosX(textStartPositionX);
		ImGui::Text("%s", text.c_str());
		ImGui::EndDownBar();
	}

	ImGui::End();
}

WidgetDownbar::~WidgetDownbar()
{

}
