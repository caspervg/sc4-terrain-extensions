#pragma once
#include "cISTETerrain.h"
#include "cISC4ViewInputControl.h"
#include "cISC4View3DWin.h"
#include "cRZAutoRefCount.h"
#include "cRZBaseString.h"
#include "Logger.h"
#include <memory>
#include <functional>
#include <map>

// Base class for all drag-based terrain tools
class GenericDragViewInputControl : public cISC4ViewInputControl {
public:
	// Parameter types that can be adjusted with scroll wheel
	enum class ParameterType {
		Primary,    // Normal scroll (e.g., height)
		Secondary,  // Alt + scroll (e.g., grade)
		Tertiary,   // Ctrl + Alt + scroll (e.g., width)
		Fine,       // Ctrl + scroll (fine adjustment of primary)
		Coarse      // Shift + scroll (coarse adjustment of primary)
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
	using DragFinishCallback = std::function<void(int32_t startX, int32_t startZ, int32_t endX, int32_t endZ)>;
	using ValidateCallback = std::function<bool(int32_t startX, int32_t startZ, int32_t endX, int32_t endZ, std::string& errorMsg)>;

	static const uint32_t GZIID_GenericDragViewInputControl = 0xA2771F6A;

private:
	uint32_t mRefCount;
	uint32_t mID;
	uint32_t mCursorIID;
	cIGZCursor* mpCursor;
	cIGZWin* mpWindow;
	cISC4View3DWin* mpView3DWin;
	cISTETerrain* mpTerrain;
	Logger* mpLogger;

	// Drag state
	bool mIsDragging;
	bool mIsInitialized;
	bool mShowingFeedback;
	int32_t mStartTileX, mStartTileZ;
	int32_t mCurrentTileX, mCurrentTileZ;
	int32_t mLastFeedbackX, mLastFeedbackY;

	// Modifier key tracking
	bool mShiftPressed;
	bool mCtrlPressed;
	bool mAltPressed;

	// Tool configuration
	std::string mToolName;
	std::string mToolDescription;
	std::map<ParameterType, Parameter> mParameters;

	// Callbacks
	DragStartCallback mOnDragStart;
	DragUpdateCallback mOnDragUpdate;
	DragFinishCallback mOnDragFinish;
	ValidateCallback mOnValidate;

	// Visual feedback IDs
	uint32_t mPrimaryTextID;
	uint32_t mSecondaryTextID;

public:
	GenericDragViewInputControl(uint32_t controlID, cISTETerrain* pTerrain, cISC4View3DWin* pView3DWin,
		const std::string& toolName, const std::string& toolDescription)
		: mRefCount(0)
		, mID(controlID)
		, mCursorIID(0)
		, mpCursor(nullptr)
		, mpWindow(nullptr)
		, mpView3DWin(pView3DWin)
		, mpTerrain(pTerrain)
		, mIsDragging(false)
		, mIsInitialized(false)
		, mShowingFeedback(false)
		, mStartTileX(0), mStartTileZ(0)
		, mCurrentTileX(0), mCurrentTileZ(0)
		, mLastFeedbackX(0), mLastFeedbackY(0)
		, mShiftPressed(false)
		, mCtrlPressed(false)
		, mAltPressed(false)
		, mToolName(toolName)
		, mToolDescription(toolDescription)
		, mPrimaryTextID(controlID + 100)
		, mSecondaryTextID(controlID + 101)
	{
		mpLogger = &Logger::GetInstance();
		mpLogger->WriteLineFormatted(LogLevel::Info, "GenericDragViewInputControl created: %s", toolName.c_str());
	}

	virtual ~GenericDragViewInputControl() = default;

	// Configuration methods
	void SetParameter(ParameterType type, const Parameter& param) {
		mParameters[type] = param;
	}

	void SetDragStartCallback(DragStartCallback callback) { mOnDragStart = callback; }
	void SetDragUpdateCallback(DragUpdateCallback callback) { mOnDragUpdate = callback; }
	void SetDragFinishCallback(DragFinishCallback callback) { mOnDragFinish = callback; }
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

	// cIGZUnknown implementation
	bool QueryInterface(uint32_t riid, void** ppvObj) override {
		if (riid == GZIID_cIGZUnknown || riid == GZIID_GenericDragViewInputControl) {
			*ppvObj = static_cast<cISC4ViewInputControl*>(this);
			AddRef();
			return true;
		}
		return false;
	}

	uint32_t AddRef() override { return ++mRefCount; }

	uint32_t Release() override {
		if (--mRefCount == 0) {
			delete this;
			return 0;
		}
		return mRefCount;
	}

	// cISC4ViewInputControl implementation
	bool Init() override {
		mIsInitialized = true;
		mpLogger->WriteLineFormatted(LogLevel::Info, "%s drag mode initialized", mToolName.c_str());
		return true;
	}

	bool Shutdown() override {
		mIsInitialized = false;
		if (mIsDragging) {
			CancelDrag();
		}
		ClearAllVisualFeedback();
		mpLogger->WriteLineFormatted(LogLevel::Info, "%s drag mode shutdown", mToolName.c_str());
		return true;
	}

	uint32_t GetID() override { return mID; }
	bool SetID(uint32_t id) override { mID = id; return true; }
	cIGZCursor* GetCursor() override { return mpCursor; }
	bool SetCursor(cIGZCursor* cursor) override { mpCursor = cursor; return true; }
	bool SetCursor(uint32_t cursor) override { mCursorIID = cursor; return true; }
	bool SetWindow(cIGZWin* window) override { mpWindow = window; return true; }
	bool IsSelfScrollingView() override { return false; }
	bool ShouldStack() override { return true; }

	// Input handlers
	bool OnCharacter(char value) override { return false; }

	bool OnKeyDown(uint32_t keyCode) override {
		switch (keyCode) {
		case 0x10: mShiftPressed = true; UpdateParameterHints(); break;
		case 0x11: mCtrlPressed = true; UpdateParameterHints(); break;
		case 0x12: mAltPressed = true; UpdateParameterHints(); break;
		case 0x1B: if (mIsDragging) { CancelDrag(); return true; } break;
		}
		return false;
	}

	bool OnKeyUp(uint32_t keyCode) override {
		switch (keyCode) {
		case 0x10: mShiftPressed = false; UpdateParameterHints(); break;
		case 0x11: mCtrlPressed = false; UpdateParameterHints(); break;
		case 0x12: mAltPressed = false; UpdateParameterHints(); break;
		}
		return false;
	}

	bool OnMouseDownL(int32_t screenX, uint32_t screenY) override {
		int32_t tileX, tileZ;
		if (ScreenToTileCoordinates(screenX, static_cast<int32_t>(screenY), tileX, tileZ)) {
			StartDrag(tileX, tileZ, screenX, static_cast<int32_t>(screenY));
			return true;
		}
		return false;
	}

	bool OnMouseDownR(int32_t screenX, uint32_t screenY) override {
		if (mIsDragging) {
			CancelDrag();
			return true;
		}
		return false;
	}

	bool OnMouseUpL(int32_t screenX, uint32_t screenY) override {
		if (mIsDragging) {
			int32_t tileX, tileZ;
			if (ScreenToTileCoordinates(screenX, static_cast<int32_t>(screenY), tileX, tileZ)) {
				FinishDrag(tileX, tileZ);
				return true;
			}
		}
		return false;
	}

	bool OnMouseUpR(int32_t screenX, uint32_t screenY) override { return false; }

	bool OnMouseMove(int32_t screenX, uint32_t screenY) override {
		mLastFeedbackX = screenX;
		mLastFeedbackY = static_cast<int32_t>(screenY);

		if (mIsDragging) {
			int32_t tileX, tileZ;
			if (ScreenToTileCoordinates(screenX, static_cast<int32_t>(screenY), tileX, tileZ)) {
				UpdateDrag(tileX, tileZ, screenX, static_cast<int32_t>(screenY));
				return true;
			}
		}
		else {
			ShowParameterFeedback(screenX, static_cast<int32_t>(screenY));
		}
		return false;
	}

	bool OnMouseWheel(int32_t delta, int32_t screenX, uint32_t screenY, int32_t unknown) override {
		if (!mIsInitialized) return false;

		ParameterType targetParam = GetScrollTargetParameter();
		auto it = mParameters.find(targetParam);

		if (it != mParameters.end()) {
			float adjustment = static_cast<float>(delta) * it->second.step;
			float newValue = it->second.value + adjustment;

			if (SetParameterValue(targetParam, newValue)) {
				mpLogger->WriteLineFormatted(LogLevel::Info,
					"%s adjusted to %.2f%s", it->second.name.c_str(),
					it->second.value, it->second.unit.c_str());

				ShowParameterFeedback(screenX, static_cast<int32_t>(screenY));
				return true;
			}
		}

		return false;
	}

	bool OnMouseExit() override {
		if (mIsDragging) {
			CancelDrag();
		}
		return true;
	}

	bool Activate() override {
		mpLogger->WriteLineFormatted(LogLevel::Info, "%s: %s", mToolName.c_str(), mToolDescription.c_str());
		ShowInitialFeedback();
		return true;
	}

	bool Deactivate() override {
		if (mIsDragging) {
			CancelDrag();
		}
		ClearAllVisualFeedback();
		mpLogger->WriteLineFormatted(LogLevel::Info, "%s deactivated", mToolName.c_str());
		return true;
	}

	bool AmCapturing() override { return mIsDragging; }

private:
	ParameterType GetScrollTargetParameter() {
		if (mCtrlPressed && mAltPressed) return ParameterType::Tertiary;
		if (mAltPressed) return ParameterType::Secondary;
		if (mShiftPressed) return ParameterType::Coarse;
		if (mCtrlPressed) return ParameterType::Fine;
		return ParameterType::Primary;
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
			return;
		}

		if (mOnDragFinish) {
			mOnDragFinish(mStartTileX, mStartTileZ, tileX, tileZ);
		}

		ShowCompletionFeedback();
	}

	void CancelDrag() {
		mIsDragging = false;
		ClearDragFeedback();
		mpLogger->WriteLineFormatted(LogLevel::Info, "%s drag cancelled", mToolName.c_str());
		ShowParameterFeedback(mLastFeedbackX, mLastFeedbackY);
	}

	void UpdateParameterHints() {
		if (mShowingFeedback) {
			ShowParameterFeedback(mLastFeedbackX, mLastFeedbackY);
		}
	}

	// Visual feedback methods
	void ShowInitialFeedback() {
		if (!mpView3DWin) return;

		cRZBaseString titleText;
		titleText.Sprintf("%s Active", mToolName.c_str());

		cRZBaseString instructionText;
		instructionText.Sprintf("%s\nScroll: Adjust parameters | Click and drag to use tool",
			mToolDescription.c_str());

		//mpView3DWin->SetCursorText(mSecondaryTextID, 0, titleText, instructionText, 0);
		mShowingFeedback = true;
	}

	void ShowParameterFeedback(int32_t screenX, int32_t screenY) {
		if (!mpView3DWin) return;

		cRZBaseString paramText = BuildParameterString();
		cRZBaseString hintText = BuildHintString();

		//mpView3DWin->SetCursorText(mPrimaryTextID, 0, paramText, hintText, 0);
		mShowingFeedback = true;
	}

	void ShowDragFeedback(int32_t screenX, int32_t screenY) {
		if (!mpView3DWin) return;

		cRZBaseString dragText = BuildDragString();
		cRZBaseString detailText = BuildDragDetailString();

		//mpView3DWin->SetCursorText(mPrimaryTextID, 0, dragText, detailText, 0);
		//mpView3DWin->SetCursorText(mSecondaryTextID, 0,
		//	cRZBaseString("Release to execute | Right-click to cancel"),
		//	BuildHintString(), 0);
		mShowingFeedback = true;
	}

	void ShowErrorFeedback(const std::string& error) {
		if (!mpView3DWin) return;

		cRZBaseString errorText;
		errorText.Sprintf("%s Error", mToolName.c_str());

		//mpView3DWin->SetCursorText(mPrimaryTextID, 0, errorText, cRZBaseString(error.c_str()), 0);
	}

	void ShowCompletionFeedback() {
		if (!mpView3DWin) return;

		cRZBaseString completionText;
		completionText.Sprintf("%s Completed!", mToolName.c_str());

		//mpView3DWin->SetCursorText(mPrimaryTextID, 0, completionText, BuildParameterString(), 0);
	}

	void ClearDragFeedback() {
		if (mpView3DWin) {
			//mpView3DWin->ClearCursorText(mSecondaryTextID);
		}
	}

	void ClearAllVisualFeedback() {
		if (mpView3DWin) {
			//mpView3DWin->ClearCursorText(mPrimaryTextID);
			//mpView3DWin->ClearCursorText(mSecondaryTextID);
		}
		mShowingFeedback = false;
	}

	// String builders
	cRZBaseString BuildParameterString() {
		cRZBaseString result;
		bool first = true;

		for (const auto& pair : mParameters) {
			if (pair.first == ParameterType::Fine || pair.first == ParameterType::Coarse) {
				continue; // Skip fine/coarse as they're just modifiers
			}

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
		ParameterType target = GetScrollTargetParameter();
		auto it = mParameters.find(target);

		cRZBaseString hint;
		if (it != mParameters.end()) {
			const char* modifier = "";
			if (mCtrlPressed && mAltPressed) modifier = "Ctrl+Alt+";
			else if (mAltPressed) modifier = "Alt+";
			else if (mShiftPressed) modifier = "Shift+";
			else if (mCtrlPressed) modifier = "Ctrl+";

			hint.Sprintf("%sScroll: %s", modifier, it->second.name.c_str());
		}
		else {
			hint.Sprintf("Scroll: Adjust parameters");
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

	bool ScreenToTileCoordinates(int32_t screenX, int32_t screenY, int32_t& tileX, int32_t& tileZ) {
		if (!mpView3DWin || !mpTerrain) return false;

		float worldCoords[3] = { 0.0f, 0.0f, 0.0f };
		bool terrainQueryState = mpView3DWin->GetTerrainQueryEnabled();

		if (mpView3DWin->PickTerrain(screenX, screenY, worldCoords, terrainQueryState)) {
			tileX = static_cast<int32_t>(worldCoords[0] / 16.0f);
			tileZ = static_cast<int32_t>(worldCoords[2] / 16.0f);

			uint32_t maxX = mpTerrain->CellCountX();
			uint32_t maxZ = mpTerrain->CellCountZ();

			tileX = std::max(0, std::min(tileX, static_cast<int32_t>(maxX - 1)));
			tileZ = std::max(0, std::min(tileZ, static_cast<int32_t>(maxZ - 1)));

			return true;
		}

		return false;
	}
};