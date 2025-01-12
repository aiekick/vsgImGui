/*
 * Copyright 2020 Stephane Cuillerdier (aka Aiekick)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifdef USE_XML_CONFIG
#    include <ConfigAbstract.h>
#endif // USE_XML_CONFIG

#include <ImGuiFileDialog.h>
#include <imgui.h>
#include <string>
#include <map>
//#define USE_SHADOW

class ThemeHelper
#ifdef USE_XML_CONFIG 
	: public conf::ConfigAbstract
#endif // USE_XML_CONFIG
{
public:
#ifdef USE_SHADOW
	static float puShadowStrength; // low value is darker than higt (0.0f - 2.0f)
	static bool puUseShadow;
	static bool puUseTextureForShadow;
#endif
	bool puShowImGuiStyleEdtor = false;

private:
	std::map<std::string, IGFD::FileStyle> prFileTypeInfos;
	ImGuiStyle prImGuiStyle;

public:
    bool init();
    void unit();

	void Draw();
	void DrawMenu();
    void ShowCustomImGuiStyleEditor(bool* vOpen, ImGuiStyle* ref = nullptr);
#ifdef USE_XML_CONFIG
	std::string getXml(const std::string& vOffset, const std::string& vUserDatas = "") override;
    bool setFromXml(tinyxml2::XMLElement* vElem, tinyxml2::XMLElement* vParent, const std::string& vUserDatas = "") override;
#endif // USE_XML_CONFIG

	ImGuiStyle GetImGuiStyle() { return prImGuiStyle; }

private:
	void ApplyStyleColorsDefault();
	void ApplyStyleColorsOrangeBlue();
	void ApplyStyleColorsGreenBlue();
	void ApplyStyleColorsClassic();
	void ApplyStyleColorsDark();
	void ApplyStyleColorsLight();
	void ApplyStyleColorsDarcula();
	void ApplyStyleColorsRedDark();

	void ApplyFileTypeColors();

	// ImGuiColorTextEdit Palette
	std::string GetStyleColorName(ImGuiCol idx);
	int GetImGuiColFromName(const std::string& vName);

public:
	ThemeHelper(); // Prevent construction
	ThemeHelper(const ThemeHelper&) {}; // Prevent construction by copying
	ThemeHelper& operator =(const ThemeHelper&) { return *this; }; // Prevent assignment
	~ThemeHelper(); // Prevent unwanted destruction
};

