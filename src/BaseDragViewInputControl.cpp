#pragma once
#include "cISTETerrain.h"
#include "cISTETerrainView.h"
#include "cSC4BaseViewInputControl.h"
#include "cISC4View3DWin.h"
#include "cRZAutoRefCount.h"
#include "cRZBaseString.h"
#include "SC4Rect.h"
#include "Logger.h"
#include <memory>
#include <functional>
#include <map>

// Base class for all drag-based terrain tools
class BaseDragViewInputControl : public cSC4BaseViewInputControl {
public:
	// SimCity 4 modifier key flags
	enum ModifierFlags : uint32_t {
		None = 0,
		Shift = 0x1,
		Ctrl = 0x2,
		Alt = 0x4,
		All = Shift | Ctrl | Alt
	};

	// Parameter types that can be adjusted with scroll wheel
	enum class ParameterType {
		First,   // Normal scroll (e.g., height)
		Second,  // Shift + scroll (e.g., grade)
		Third,   // Ctrl + Scroll (e.g., width)
		Fourth,  // Alt + scroll (e.g., ?)
	};

	// Parameter definition
	struct Parameter {
		std::string name;
		float value;
		float minValue;
		float maxValue;
		float step;
		std::string unit;

		// Default constructor for std::map compatibility
		Parameter() : name(""), value(0.0f), minValue(0.0f), maxValue(0.0f), step(0.0f), unit("") {}

		Parameter(const std::string& n, float v, float min, float max, float s, const std::string& u = "")
			: name(n), value(v), minValue(min), maxValue(max), step(s), unit(u) {
		}
	};

	// Callbacks for tool-specific behavior
	using DragStartCallback = std::function<void(int32_t tileX, int32_t tileZ)>;
	using DragUpdateCallback = std::function<void(int32_t startX, int32_t startZ, int32_t currentX, int32_t currentZ)>;
	using DragCancelCallback = std::function<void(int32_t startX, int32_t startZ, int32_t currentX, int32_t currentZ)>;
	using DragFinishCallback = std::function<void(int32_t startX, int32_t startZ, int32_t endX, int32_t endZ)>;
	using ValidateCallback = std::function<bool(int32_t startX, int32_t startZ, int32_t endX, int32_t endZ, std::string& errorMsg)>;

private:
	cISC4View3DWin* mpView3DWin;
	cIGZWin* mpWindow;


	// Drag state
	bool mIsDragging;
	bool mShowingFeedback;
	int32_t mStartTileX, mStartTileZ;
	int32_t mCurrentTileX, mCurrentTileZ;
	int32_t mLastFeedbackX, mLastFeedbackY;

	// Current modifier state (cached from last event)
	uint32_t mCurrentModifiers;

	// Tool configuration
	std::string mToolName;
	std::string mToolDescription;
	std::map<ParameterType, Parameter> mParameters;

	// Callbacks
	DragStartCallback mOnDragStart;
	DragUpdateCallback mOnDragUpdate;
	DragCancelCallback mOnDragCancel;
	DragFinishCallback mOnDragFinish;
	ValidateCallback mOnValidate;

	// Visual feedback IDs
	uint32_t mPrimaryTextID;
	uint32_t mSecondaryTextID;

public:
	BaseDragViewInputControl(uint32_t controlID, cISTETerrain* pTerrain, cIGZWin* pWindow, cISC4View3DWin* pView3DWin,
		const std::string& toolName, const std::string& toolDescription)
		: cSC4BaseViewInputControl(controlID)
		, mpView3DWin(pView3DWin)
		, mpWindow(pWindow)
		, mpTerrain(pTerrain)
		, mIsDragging(false)
		, mShowingFeedback(false)
		, mStartTileX(0), mStartTileZ(0)
		, mCurrentTileX(0), mCurrentTileZ(0)
		, mLastFeedbackX(0), mLastFeedbackY(0)
		, mCurrentModifiers(0)
		, mToolName(toolName)
		, mToolDescription(toolDescription)
		, mPrimaryTextID(controlID + 100)
		, mSecondaryTextID(controlID + 101)
	{
		mpLogger = &Logger::GetInstance();
		mpLogger->WriteLineFormatted(LogLevel::Info, "GenericDragViewInputControl created: %s", toolName.c_str());
	}

	// Configuration methods
	void SetParameter(ParameterType type, const Parameter& param) {
		mParameters[type] = param;
	}

	void SetDragStartCallback(DragStartCallback callback) { mOnDragStart = callback; }
	void SetDragUpdateCallback(DragUpdateCallback callback) { mOnDragUpdate = callback; }
	void SetDragFinishCallback(DragFinishCallback callback) { mOnDragFinish = callback; }
	void SetDragCancelCallback(DragCancelCallback callback) { mOnDragCancel = callback; }
	void SetValidateCallback(ValidateCallback callback) { mOnValidate = callback; }

	// Parameter access
	float GetParameterValue(ParameterType type) const {
		auto it = mParameters.find(type);
		return (it != mParameters.end()) ? it->second.value : 0.0f;
	}

	bool SetParameterValue(ParameterType type, float value) {
		auto it = mParameters.find(type);
		if (it != mParameters.end()) {
			float clampedValue = std::max(it->second.minValue,
				std::min(value, it->second.maxValue));
			if (clampedValue != it->second.value) {
				it->second.value = clampedValue;
				return true;
			}
		}
		return false;
	}

	bool OnKeyDown(int32_t vkCode, uint32_t modifiers) override {
		mpLogger->WriteLineFormatted(LogLevel::Info, "OnKeyDown: keyCode=0x%X, modifiers=0x%X", vkCode, modifiers);

		mCurrentModifiers = modifiers;
		switch (vkCode) {
			case 0x1B: if (mIsDragging) { CancelDrag(); return true; } break; // ESC
		}
		return false;
	}

	bool OnMouseDownL(int32_t screenX, int32_t screenZ, uint32_t modifiers) override {
		if (!IsOnTop()) return false;
		
		uint32_t tileX, tileZ;
		if (ScreenToTileCoordinates(screenX, screenZ, tileX, tileZ)) {
			StartDrag(tileX, tileZ, screenX, screenZ);
			return true;
		}
		return false;
	}

	bool OnMouseDownR(int32_t x, int32_t z, uint32_t modifiers) override {
		if (!IsOnTop()) return false;
		
		if (mIsDragging) {
			CancelDrag();
			return true;
		}
		return false;
	}

	bool OnMouseUpL(int32_t screenX, int32_t screenZ, uint32_t modifiers) override {
		if (!IsOnTop()) return false;
		
		if (mIsDragging) {
			uint32_t tileX, tileZ;
			if (ScreenToTileCoordinates(screenX, screenZ, tileX, tileZ)) {
				FinishDrag(tileX, tileZ);
				return true;
			}
		}
		return false;
	}

	bool OnMouseMove(int32_t screenX, int32_t screenZ, uint32_t modifiers) override {
		if (!IsOnTop()) return false;
		
		mCurrentModifiers = modifiers;
		mLastFeedbackX = screenX;
		mLastFeedbackY = screenZ;

		if (mIsDragging) {
			uint32_t tileX, tileZ;
			if (ScreenToTileCoordinates(screenX, screenZ, tileX, tileZ)) {
				UpdateDrag(tileX, tileZ, screenX, screenZ);
			}
		}
		else {
			ShowParameterFeedback(screenX, screenZ);
		}
		
		return true;
	}

	bool OnMouseWheel(int32_t screenX, int32_t screenZ, uint32_t modifiers, int32_t wheelDelta) override {
		if (!IsOnTop()) return false;

		if (!initialized) return false;
		if (wheelDelta == 0) return false;

		// Only handle mouse wheel when Alt is pressed, otherwise let game handle zoom
		bool altPressed = (modifiers & ModifierFlags::Alt) != 0;
		if (!altPressed) {
			return false;  // Let the game handle normal zoom
		}

		mCurrentModifiers = modifiers;
		ParameterType targetParam = GetScrollTargetParameter(modifiers);
		auto it = mParameters.find(targetParam);

		if (it != mParameters.end()) {
			float adjustment = (wheelDelta > 0 ? 1.0f : -1.0f) * it->second.step;
			float newValue = it->second.value + adjustment;

			if (SetParameterValue(targetParam, newValue)) {
				mpLogger->WriteLineFormatted(LogLevel::Info,
					"%s adjusted to %.2f%s", it->second.name.c_str(),
					it->second.value, it->second.unit.c_str());

				ShowParameterFeedback(screenX, screenZ);
				return true;
			}
		}

		return false;
	}

	bool OnMouseExit() override {
		mpLogger->WriteLineFormatted(LogLevel::Info, "OnMouseExit");

		if (mIsDragging) {
			CancelDrag();
		}
		return true;
	}

private:
	ParameterType GetScrollTargetParameter(uint32_t modifiers) {
		bool ctrlPressed = (modifiers & ModifierFlags::Ctrl) != 0;
		bool shiftPressed = (modifiers & ModifierFlags::Shift) != 0;
		bool altPressed = (modifiers & ModifierFlags::Alt) != 0;
		
		mpLogger->WriteLineFormatted(LogLevel::Info, 
			"GetScrollTargetParameter: modifiers=0x%X, ctrl=%d, shift=%d, alt=%d", 
			modifiers, ctrlPressed, shiftPressed, altPressed);
		
		// Alt is required for any parameter adjustment
		// Alt+Ctrl+Shift = Fourth, Alt+Ctrl = Third, Alt+Shift = Second, Alt = First
		if (altPressed) {
			if (ctrlPressed && shiftPressed) {
				mpLogger->WriteLineFormatted(LogLevel::Info, "Selected parameter: Fourth (Alt+Ctrl+Shift)");
				return ParameterType::Fourth;
			} else if (ctrlPressed) {
				mpLogger->WriteLineFormatted(LogLevel::Info, "Selected parameter: Third (Alt+Ctrl)");
				return ParameterType::Third;
			} else if (shiftPressed) {
				mpLogger->WriteLineFormatted(LogLevel::Info, "Selected parameter: Second (Alt+Shift)");
				return ParameterType::Second;
			} else {
				mpLogger->WriteLineFormatted(LogLevel::Info, "Selected parameter: First (Alt only)");
				return ParameterType::First;
			}
		}
		
		// This should not be reached since we only call this when Alt is pressed
		mpLogger->WriteLineFormatted(LogLevel::Info, "Selected parameter: First (fallback)");
		return ParameterType::First;
	}

	void StartDrag(int32_t tileX, int32_t tileZ, int32_t screenX, int32_t screenY) {
		mIsDragging = true;
		mStartTileX = tileX;
		mStartTileZ = tileZ;
		mCurrentTileX = tileX;
		mCurrentTileZ = tileZ;

		if (mOnDragStart) {
			mOnDragStart(tileX, tileZ);
		}

		ShowDragFeedback(screenX, screenY);
	}

	void UpdateDrag(int32_t tileX, int32_t tileZ, int32_t screenX, int32_t screenY) {
		if (mCurrentTileX != tileX || mCurrentTileZ != tileZ) {
			mCurrentTileX = tileX;
			mCurrentTileZ = tileZ;

			if (mOnDragUpdate) {
				mOnDragUpdate(mStartTileX, mStartTileZ, tileX, tileZ);
			}

			ShowDragFeedback(screenX, screenY);
		}
	}

	void FinishDrag(int32_t tileX, int32_t tileZ) {
		mIsDragging = false;
		ClearDragFeedback();

		// Validate the operation
		std::string errorMsg;
		if (mOnValidate && !mOnValidate(mStartTileX, mStartTileZ, tileX, tileZ, errorMsg)) {
			mpLogger->WriteLineFormatted(LogLevel::Info, "%s validation failed: %s",
				mToolName.c_str(), errorMsg.c_str());
			ShowErrorFeedback(errorMsg);
			mOnDragCancel(mStartTileX, mStartTileZ, tileX, tileZ);
			return;
		}

		if (mOnDragFinish) {
			mOnDragFinish(mStartTileX, mStartTileZ, tileX, tileZ);
		}

		ShowCompletionFeedback();
	}

	void CancelDrag() {
		mIsDragging = false;
		mOnDragCancel(mStartTileX, mStartTileZ, mCurrentTileX, mCurrentTileZ);
		ClearDragFeedback();
		mpLogger->WriteLineFormatted(LogLevel::Info, "%s drag cancelled", mToolName.c_str());
		ShowParameterFeedback(mLastFeedbackX, mLastFeedbackY);
	}

	// Visual feedback methods
	void ShowInitialFeedback() {
		if (!mpView3DWin) return;

		cRZBaseString titleText;
		titleText.Sprintf("%s Active", mToolName.c_str());

		cRZBaseString instructionText;
		instructionText.Sprintf("%s\nScroll: Adjust parameters | Click and drag to use tool",
			mToolDescription.c_str());

		mpView3DWin->SetCursorText(mSecondaryTextID, 0, &instructionText, &titleText, 0);
		mShowingFeedback = true;
	}

	void ShowParameterFeedback(int32_t screenX, int32_t screenY) {
		if (!mpView3DWin) return;

		cRZBaseString paramText = BuildParameterString();
		cRZBaseString hintText = BuildHintString();

		mpView3DWin->SetCursorText(mPrimaryTextID, 0, &paramText, &hintText, 0);
		mShowingFeedback = true;
	}

	void ShowDragFeedback(int32_t screenX, int32_t screenY) {
		if (!mpView3DWin) return;

		cRZBaseString dragText = BuildDragString();
		cRZBaseString detailText = BuildDragDetailString();

		mpView3DWin->SetCursorText(mPrimaryTextID, 0, &detailText, &dragText, 0);
		
		cRZBaseString cancelText("Release to execute | Right-click to cancel");
		cRZBaseString hintText = BuildHintString();
		mpView3DWin->SetCursorText(mSecondaryTextID, 0, &hintText, &cancelText, 0);
		mShowingFeedback = true;
	}

	void ShowErrorFeedback(const std::string& error) {
		if (!mpView3DWin) return;

		cRZBaseString errorText;
		errorText.Sprintf("%s Error", mToolName.c_str());

		cRZBaseString errorDetailText(error.c_str());
		mpView3DWin->SetCursorText(mPrimaryTextID, 0, &errorText, &errorDetailText, 0);
	}

	void ShowCompletionFeedback() {
		if (!mpView3DWin) return;

		cRZBaseString completionText;
		completionText.Sprintf("%s Completed!", mToolName.c_str());

		cRZBaseString paramText = BuildParameterString();
		mpView3DWin->SetCursorText(mPrimaryTextID, 0, &paramText, &completionText, 0);
	}

	void ClearDragFeedback() {
		if (mpView3DWin) {
			mpView3DWin->ClearCursorText(mSecondaryTextID);
		}
	}

	void ClearAllVisualFeedback() {
		if (mpView3DWin) {
			mpView3DWin->ClearCursorText(mPrimaryTextID);
			mpView3DWin->ClearCursorText(mSecondaryTextID);
		}
		mShowingFeedback = false;
	}

	// String builders
	cRZBaseString BuildParameterString() {
		cRZBaseString result;
		bool first = true;

		for (const auto& pair : mParameters) {
			if (!first) result.Append(cRZBaseString(" | "));

			cRZBaseString paramStr;
			paramStr.Sprintf("%s: %.1f%s", pair.second.name.c_str(),
				pair.second.value, pair.second.unit.c_str());
			result.Append(paramStr);
			first = false;
		}

		return result;
	}

	cRZBaseString BuildHintString() {
		cRZBaseString hint;
		bool ctrlPressed = (mCurrentModifiers & ModifierFlags::Ctrl) != 0;
		bool shiftPressed = (mCurrentModifiers & ModifierFlags::Shift) != 0;
		bool altPressed = (mCurrentModifiers & ModifierFlags::Alt) != 0;
		
		if (altPressed) {
			// Alt is pressed, show which parameter will be adjusted
			ParameterType target = GetScrollTargetParameter(mCurrentModifiers);
			auto it = mParameters.find(target);
			
			if (it != mParameters.end()) {
				const char* modifier = "";
				if (ctrlPressed && shiftPressed) modifier = "Alt+Ctrl+Shift+";
				else if (ctrlPressed) modifier = "Alt+Ctrl+";
				else if (shiftPressed) modifier = "Alt+Shift+";
				else modifier = "Alt+";

				hint.Sprintf("%sScroll: %s", modifier, it->second.name.c_str());
			} else {
				hint.Sprintf("Alt+Scroll: Adjust parameters");
			}
		} else {
			// Alt not pressed, show instructions
			hint.Sprintf("Alt+Scroll: Adjust parameters | Scroll: Zoom");
		}

		return hint;
	}

	cRZBaseString BuildDragString() {
		int32_t dx = mCurrentTileX - mStartTileX;
		int32_t dz = mCurrentTileZ - mStartTileZ;
		float distance = std::sqrt(static_cast<float>(dx * dx + dz * dz));

		cRZBaseString result;
		result.Sprintf("%s: %.1f tiles | %s", mToolName.c_str(), distance,
			BuildParameterString().Data());

		return result;
	}

	cRZBaseString BuildDragDetailString() {
		cRZBaseString result;
		result.Sprintf("Start: (%d,%d) | End: (%d,%d)",
			mStartTileX, mStartTileZ, mCurrentTileX, mCurrentTileZ);

		return result;
	}

	bool ScreenToTileCoordinates(int32_t screenX, int32_t screenY, uint32_t& tileX, uint32_t& tileZ) {
		if (!mpView3DWin || !mpTerrain) {
			mpLogger->WriteLineFormatted(LogLevel::Error, "ScreenToTileCoordinates: null pointers - View3D:%p Terrain:%p", mpView3DWin, mpTerrain);
			return false;
		}

		float worldCoords[3] = { 0.0f, 0.0f, 0.0f };
		bool terrainQueryState = mpView3DWin->GetTerrainQueryEnabled();
		
		mpLogger->WriteLineFormatted(LogLevel::Trace, "ScreenToTileCoordinates: calling PickTerrain(%d,%d)", screenX, screenY);

		bool pickResult = false;
		pickResult = mpView3DWin->PickTerrain(screenX, screenY, worldCoords, terrainQueryState);

		if (pickResult) {
			tileX = static_cast<uint32_t>(worldCoords[0] / 16.0f);
			tileZ = static_cast<uint32_t>(worldCoords[2] / 16.0f);

			uint32_t maxX = mpTerrain->CellCountX();
			uint32_t maxZ = mpTerrain->CellCountZ();

			tileX = std::max(static_cast<uint32_t>(0), std::min(tileX, static_cast<uint32_t>(maxX - 1)));
			tileZ = std::max(static_cast<uint32_t>(0), std::min(tileZ, static_cast<uint32_t>(maxZ - 1)));

			mpLogger->WriteLineFormatted(LogLevel::Trace, "ScreenToTileCoordinates: success - tile(%d,%d)", tileX, tileZ);
			return true;
		}

		mpLogger->WriteLineFormatted(LogLevel::Error, "ScreenToTileCoordinates: PickTerrain failed");
		return false;
	}
protected:
	Logger* mpLogger;
	cISTETerrain* mpTerrain;

	bool MarkSelected(uint32_t startX, uint32_t startZ, uint32_t endX, uint32_t endZ, cISTETerrain::eHilightColorType hilightColor = cISTETerrain::eHilightColorType::Red, bool clearOthers = true) {
		if (startX < 0 || startZ < 0 || endX < 0 || endZ < 0 ||
			startX > mpTerrain->CellCountX() - 1 || startZ > mpTerrain->CellCountZ() - 1 || endX > mpTerrain->CellCountX() - 1 || endZ > mpTerrain->CellCountZ()) {
			mpLogger->WriteLine(LogLevel::Error, "MarkSelected: invalid coordinates, out of bounds");
			return false;
		}

		SC4Rect<uint32_t> rect = { std::min(startX, endX), std::min(startZ, endZ), std::max(startX, endX), std::max(startZ, endZ)};
		this->MarkSelected(rect, hilightColor, clearOthers);
	}

	bool MarkSelected(SC4Rect<uint32_t> rect, cISTETerrain::eHilightColorType hilightColor = cISTETerrain::eHilightColorType::Red, bool clearOthers = true) {
		if (!mpTerrain || !mpTerrain->GetView()) {
			mpLogger->WriteLine(LogLevel::Error, "MarkSelected: terrain or terrain view is null");
			return false;
		}

		return mpTerrain->GetView()->MarkSelected(rect, hilightColor, clearOthers);
	}

	void ClearCurrentSelections() {
		mpTerrain->GetView()->ClearCurrentSelections();
	}
};