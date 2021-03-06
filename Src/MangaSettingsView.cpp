#include "MangaSettingsView.h"
#include "Mangaroll.h"
#include "GLES3\gl3_loader.h"
#include "DefaultComponent.h"
#include "VRMenuMgr.h"
#include "Helpers.h"
#include "Kernel\OVR_String_Utils.h"
#include "GazeUpdateComponent.h"
#include "Config.h"

namespace OvrMangaroll {

	float PixelScale(const float x)
	{
		return x * VRMenuObject::DEFAULT_TEXEL_SCALE;
	}
	Vector3f PixelPos(const float x, const float y, const float z)
	{
		return Vector3f(PixelScale(x), PixelScale(y), PixelScale(z));
	}

	void OnText(ScrubBarComponent *button, void *object, UILabel *label, int progress) {
		label->SetText(String::Format(AppState::Strings->PageNo.ToCStr(), progress + 1));
	}

	void OnCloseClick(UIButton *button, void *obj) {
		((MangaSettingsView *)obj)->HideGUI();
	}

	float lastProgressClick = 0;
	void OnPageProgressClick(ScrubBarComponent *slider, void *object, float progress) {
		if (Time::Elapsed - lastProgressClick > 0.1f) {
			((MangaSettingsView *)object)->SetPageProgress(progress);
			lastProgressClick = Time::Elapsed;
		}
	}
	void OnContrastClick(ScrubBarComponent *slider, void *object, float progress) {
		slider->SetProgress(progress);
		AppState::Conf->Contrast = progress * 2;

		/*GlProgram *prog = ShaderManager::Instance()->Get(PAGE_SHADER_NAME);
		glUseProgram(prog->program);
		glUniform1f(glGetUniformLocation(prog->program, "Contrast"), progress*2);*/
	}

	void OnBrightnessClick(ScrubBarComponent *slider, void *object, float progress) {
		slider->SetProgress(progress);

		//GlProgram *prog = ShaderManager::Instance()->Get(PAGE_SHADER_NAME);
		AppState::Conf->Brightness = (progress - 0.5f) * 2;

		//glUseProgram(prog->program);
		//glUniform1f(glGetUniformLocation(prog->program, "Brightness"), (progress - 0.5f)*2);
	}

	MangaSettingsView::MangaSettingsView(Mangaroll *app) :
		View("MangaSettingsView")
		, _Mangaroll(app)
		, _CloseRequested(false)
		, _Menu(NULL)
		, _OrientationContainer(NULL)
		, _OrientationOffset()
		, _CenterContainer(NULL)
		, _LeftContainer(NULL)
		, _RightContainer(NULL)
		, _TitleLabel(NULL)
		, _PageLabel(NULL)
		, _GammaSlider(NULL)
		, _ContrastSlider(NULL)
		, _BrightnessSlider(NULL)
		, _BrightnessLabel(NULL)
		, _Fader(0)
		, _ProgressComponent()
		, _PageSeekLabel(NULL)
		, _ProgressBG(NULL)
		, _ProgressFG(NULL)
		, _ProgressBGTexture()
		, _ProgressFGTexture()
		, _FXBGTexture()
		, _MainBGTexture()
		, _OptionsBGTexture()
		, _SeekPosTexture()
		, _MainContainerWidth(300.0f)
	{
	}

	MangaSettingsView::~MangaSettingsView() {

	}

	bool MangaSettingsView::OnKeyEvent(const int keyCode, const int repeatCount, const KeyEventType eventType) {
		if (keyCode == OVR_KEY_BACK && eventType == KeyEventType::KEY_EVENT_SHORT_PRESS) {
			if (_Menu->IsOpen()) {
				_Mangaroll->SelectManga();
			}
			else {
				ShowGUI();
			}
			return true;
		}
		return false;
	}
	Matrix4f MangaSettingsView::Frame(const VrFrame & vrFrame) {		
		UpdateMenuState();
		
		if (vrFrame.Input.buttonPressed & BUTTON_SWIPE_DOWN) {
			HideGUI();
		}

		return _Mangaroll->Carousel.Frame(vrFrame);
	}
	Matrix4f MangaSettingsView::GetEyeViewMatrix(const int eye) const {
		return _Mangaroll->Carousel.GetEyeViewMatrix(eye);
	}
	Matrix4f MangaSettingsView::GetEyeProjectionMatrix(const int eye, const float fovDegreesX, const float fovDegreesY) const {
		return _Mangaroll->Carousel.GetEyeProjectionMatrix(eye, fovDegreesX, fovDegreesY);
	}
	Matrix4f MangaSettingsView::DrawEyeView(const int eye, const float fovDegreesX, const float fovDegreesY, ovrFrameParms & frameParms) {

		return _Mangaroll->Carousel.DrawEyeView(eye, fovDegreesX, fovDegreesY, frameParms);
	}

	void MangaSettingsView::OneTimeInit(const char * launchIntent) {
		// BUILD GUI
		const Quatf forward(Vector3f(0.0f, 1.0f, 0.0f), 0.0f);
		const Quatf leftTilt(Vector3f(0, 1, 0), 0.8f);
		const Quatf rightTilt(Vector3f(0, 1, 0), -0.8f);

		OvrGuiSys &gui = _Mangaroll->GetGuiSys();

		_ProgressBGTexture.LoadTextureFromApplicationPackage("assets/progress_bg.png");
		_ProgressFGTexture.LoadTextureFromApplicationPackage("assets/fill.png");
		_FXBGTexture.LoadTextureFromApplicationPackage("assets/effects_window.png");
		_OptionsBGTexture.LoadTextureFromApplicationPackage("assets/options_window.png");
		_MainBGTexture.LoadTextureFromApplicationPackage("assets/main_window.png");
		_SeekPosTexture.LoadTextureFromApplicationPackage("assets/img_seek_position.png");

		_Menu = new UIMenu(gui);
		_Menu->Create("MangaMenu");
		_Menu->SetFlags(VRMenuFlags_t(VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP));
		_OrientationContainer = new UIContainer(gui);
		_OrientationContainer->AddToMenu(_Menu);

		_CenterContainer = new UIContainer(gui);
		_CenterContainer->AddToMenu(_Menu, _OrientationContainer);
		//_CenterContainer->AddFlags(VRMENUOBJECT_RENDER_HIERARCHY_ORDER);
		_CenterContainer->SetLocalPose(forward, Vector3f(0, 0, -.9f));

		_LeftContainer = new UIContainer(gui);
		_LeftContainer->AddToMenu(_Menu, _OrientationContainer);
		_LeftContainer->SetLocalPose(leftTilt, Vector3f(-.4f, 0, -.7f));
		
		_RightContainer = new UIContainer(gui);
		_RightContainer->AddToMenu(_Menu, _OrientationContainer);
		_RightContainer->SetLocalPose(rightTilt, Vector3f(.4f, 0, -.7f));

		// #########  MAIN WINDOW  ###########

		_MainBG = new UIImage(gui);
		_MainBG->AddToMenu(_Menu, _CenterContainer);
		_MainBG->SetLocalPosition(PixelPos(0, -20, -20));
		_MainBG->SetImage(0, SURFACE_TEXTURE_DIFFUSE, _MainBGTexture, _MainContainerWidth, _MainContainerWidth / _MainBGTexture.Width * _MainBGTexture.Height);

		_TitleLabel = new UILabel(gui);
		_TitleLabel->AddToMenu(_Menu, _CenterContainer);
		// It's unknown what these parms actually do... but well, it works. "alpha" seems to control the thickness of the outline
		_TitleLabel->SetFontParms(VRMenuFontParms(true, true, false, false, true, 0.5f, 0.4f, 1));
		_TitleLabel->SetLocalPose(forward, Vector3f(0, 0.05f, 0));
		_TitleLabel->SetFontScale(0.6f);
		_TitleLabel->SetText("");
		

		OvrDefaultComponent *defaultComponent = new OvrDefaultComponent(
			Vector3f(0),
			1.05f,
			0.25f,
			0
			);
		GazeUpdaterComponent *component = new GazeUpdaterComponent();


		_PageLabel = new UILabel(gui);
		_PageLabel->AddToMenu(_Menu, _CenterContainer);
		_PageLabel->SetFontParms(VRMenuFontParms(true, true, false, false, true, 0.5f, 0.4f, 1));
		_PageLabel->SetLocalPose(forward, Vector3f(0, -0.05f, 0));
		_PageLabel->SetFontScale(0.4f);
		_PageLabel->SetFlags(VRMENUOBJECT_DONT_HIT_ALL);
		_PageLabel->SetText("");

		component->HandlesEvent(VRMenuEventFlags_t(VRMENU_EVENT_FOCUS_GAINED) /*| VRMENU_EVENT_FOCUS_LOST | VRMENU_EVENT_FRAME_UPDATE*/);
		_CenterContainer->AddComponent(component);


		float barWidth = 7 * 30;
		float barHeight = 30;

		_ProgressBG = new UIImage(gui);
		_ProgressBG->AddToMenu(_Menu, _CenterContainer);
		_ProgressBG->SetImage(0, eSurfaceTextureType::SURFACE_TEXTURE_DIFFUSE, _ProgressBGTexture, barWidth, barHeight);
		_ProgressBG->SetLocalPose(forward, Vector3f(0, -0.1f, 0));
		_ProgressBG->AddComponent(&_ProgressComponent);
		_ProgressBG->GetMenuObject()->AddFlags(VRMENUOBJECT_RENDER_HIERARCHY_ORDER);

		_ProgressFG = new UIImage(gui);
		_ProgressFG->AddToMenu(_Menu, _ProgressBG);
		_ProgressFG->SetLocalPosition(PixelPos(-3, 0, 1));
		_ProgressFG->SetImage(0, SURFACE_TEXTURE_ADDITIVE, _ProgressFGTexture, 0, 0);
		_ProgressFG->GetMenuObject()->AddFlags(VRMENUOBJECT_DONT_HIT_ALL);
		//_ProgressFG->SetColor(Vector4f(0.12f, 0.3f, 0.06f, 1));
		_ProgressFG->SetColor(Vector4f(0, 0.2f, 0.2f, 1));


		_PageSeekLabel = new UILabel(gui);
		_PageSeekLabel->AddToMenu(_Menu, _ProgressBG);
		_PageSeekLabel->GetMenuObject()->AddFlags(VRMENUOBJECT_DONT_HIT_ALL);
		_PageSeekLabel->SetLocalPose(forward, Vector3f(0, 0, 0));
		_PageSeekLabel->SetFontScale(0.3f);
		_PageSeekLabel->SetText("");
		_PageSeekLabel->SetTextOffset(Vector3f(0, -0.065f, 0.02f));

		_SeekPos = new UIImage(gui);
		_SeekPos->AddToMenu(_Menu, _PageSeekLabel);
		_SeekPos->SetLocalPosition(Vector3f(0, -0.06f, 0));
		_SeekPos->SetColor(Vector4f(0, 0.2f, 0.2f, 1.0f));
		_SeekPos->SetImage(0, SURFACE_TEXTURE_DIFFUSE, _SeekPosTexture, 70, 30);


		_ProgressComponent.SetWidgets(_Menu, _ProgressBG, _ProgressFG, _PageLabel, _PageSeekLabel, barWidth - 6, barHeight - 3);
		_ProgressComponent.SetOnText(OnText, this);
		_ProgressComponent.SetProgress(0.5f);
		_ProgressComponent.SetOnClick(OnPageProgressClick, this);
	

		// #########  EFFECTS  ###########
		_FXBG = new UIImage(gui);
		_FXBG->AddToMenu(_Menu, _LeftContainer);
		_FXBG->SetLocalPosition(PixelPos(0, -50, -1));
		_FXBG->SetImage(0, SURFACE_TEXTURE_DIFFUSE, _FXBGTexture, 130.0f, 130.0f / _FXBGTexture.Width * _FXBGTexture.Height);
		
		_ContrastLabel = new UILabel(gui);
		_ContrastLabel->AddToMenu(_Menu, _LeftContainer);
		_ContrastLabel->SetFontParms(VRMenuFontParms(true, true, false, false, true, 0.5f, 0.4f, 0.3f));
		_ContrastLabel->SetText(AppState::Strings->PrefContrast);
		_ContrastLabel->SetMargin(UIRectf(5));
		_ContrastLabel->CalculateTextDimensions();
				
		_ContrastSlider = new UIScrubBar(gui, 100, 20);
		_ContrastSlider->AddToMenu(_Menu, _LeftContainer);
		_ContrastSlider->AlignToMargin(RectPosition::BOTTOM, _ContrastLabel, RectPosition::TOP);
		_ContrastSlider->GetComponent().SetProgress(0.5f);
		_ContrastSlider->SetColor(Vector4f(0.596f, 0.051f, 0.051f, 1));
		_ContrastSlider->GetComponent().SetOnClick(OnContrastClick, this);
		_ContrastSlider->SetMargin(UIRectf(3));

		_BrightnessLabel = new UILabel(gui);
		_BrightnessLabel->AddToMenu(_Menu, _LeftContainer);
		_BrightnessLabel->SetFontParms(VRMenuFontParms(true, true, false, false, true, 0.5f, 0.4f, 0.3f));
		_BrightnessLabel->SetText(AppState::Strings->PrefBrightness);
		_BrightnessLabel->SetMargin(UIRectf(3));
		_BrightnessLabel->CalculateTextDimensions();
		_BrightnessLabel->SetLocalPosition(Vector3f(0, -.1f, 0));

		_BrightnessSlider = new UIScrubBar(gui, 100, 20);
		_BrightnessSlider->AddToMenu(_Menu, _LeftContainer);
		_BrightnessSlider->AlignToMargin(RectPosition::BOTTOM, _BrightnessLabel, RectPosition::TOP);
		_BrightnessSlider->GetComponent().SetProgress(0.5f);
		_BrightnessSlider->SetColor(Vector4f(0.596f, 0.051f, 0.051f, 1));
		_BrightnessSlider->GetComponent().SetOnClick(OnBrightnessClick, this);

		// #########  OPTIONS  ###########

		//_RightContainer->AddFlags(VRMENUOBJECT_RENDER_HIERARCHY_ORDER);
		_OptionsBG = new UIImage(gui);
		_OptionsBG->AddToMenu(_Menu, _RightContainer);
		_OptionsBG->SetLocalPosition(PixelPos(0, -50, -10));
		_OptionsBG->SetImage(0, SURFACE_TEXTURE_DIFFUSE, _OptionsBGTexture, 130.0f, 130.0f / _OptionsBGTexture.Width * _OptionsBGTexture.Height);
		_OptionsBG->AddFlags(VRMENUOBJECT_RENDER_HIERARCHY_ORDER);

		float toggleHeight = 25.0f;

		_ShaderToggle = new UICheckbox(gui, 120.0f, toggleHeight);
		_ShaderToggle->AddToMenu(_Menu, _OptionsBG);
		_ShaderToggle->SetLocalPosition(PixelPos(0, 50, 10)); // Compensate
		_ShaderToggle->SetText(AppState::Strings->PrefTransparent);
		_ShaderToggle->SetOnValueChanged(OnShaderChanged, this);

		_ReadDirToggle = new UICheckbox(gui, 120.0f, toggleHeight);
		_ReadDirToggle->AddToMenu(_Menu, _OptionsBG);
		_ReadDirToggle->SetText(AppState::Strings->PrefReadDir);
		_ReadDirToggle->SetLocalPosition(_ShaderToggle->GetLocalPosition() + Vector3f(0, -PixelScale(toggleHeight + 5), 0));
		_ReadDirToggle->SetOnValueChanged(OnReadDirChanged, this);
		//_ReadDirToggle->AlignToMargin(TOP, _ShaderToggle, BOTTOM);

		_AutoToggle = new UICheckbox(gui, 120.0f, toggleHeight);
		_AutoToggle->AddToMenu(_Menu, _OptionsBG);
		_AutoToggle->SetText(AppState::Strings->PrefAutoProgress);
		_AutoToggle->SetLocalPosition(_ReadDirToggle->GetLocalPosition() - Vector3f(0, PixelScale(toggleHeight + 5), 0));
		_AutoToggle->SetOnValueChanged(OnAutoRotateChanged, this);

		//_AutoToggle->AlignToMargin(TOP, _ReadDirToggle, BOTTOM);
		//WARN("RECT: %.2f", _ShaderToggle->GetRect().GetHeight());

		_HelperToggle = new UICheckbox(gui, 120.0f, toggleHeight);
		_HelperToggle->AddToMenu(_Menu, _OptionsBG);
		_HelperToggle->SetText(AppState::Strings->PrefHeadMotions);
		_HelperToggle->SetLocalPosition(_AutoToggle->GetLocalPosition() - Vector3f(0, PixelScale(toggleHeight + 5), 0));
		_HelperToggle->SetOnValueChanged(OnGuideChanged, this);

		//_HelperToggle->AlignTo(TOP, _ReadDirToggle, BOTTOM);


		_CloseTexture.LoadTextureFromApplicationPackage("assets/close.png");
		_CloseButton = new UIButton(gui);
		_CloseButton->AddToMenu(_Menu, _RightContainer);
		_CloseButton->SetButtonImages(_CloseTexture, _CloseTexture, _CloseTexture);
		_CloseButton->SetOnClick(OnCloseClick, this);
		_CloseButton->SetLocalPosition(Vector3f(0, 0, 0));
		_CloseButton->SetDimensions(Vector2f(50, 50));
		_CloseButton->SetButtonColors(Vector4f(0.8f, 0.8f, 0.8f, 1), Vector4f(1, 1, 1, 1), Vector4f(1, 1, 1, 1));
		_CloseButton->AddComponent(component);
		_CloseButton->SetVisible(false);
		_CloseButton->AddComponent(defaultComponent);

	}
	void MangaSettingsView::SetPageProgress(float progress) {
		int prevPage = _ProgressComponent.GetMax() * _ProgressComponent.GetProgress();
		_ProgressComponent.SetProgress(progress);
		int newPage = _ProgressComponent.GetMax() * progress;
		
		if (prevPage != newPage)
			_Mangaroll->CurrentManga->SetProgress(progress * _Mangaroll->CurrentManga->GetCount());
	}

	void MangaSettingsView::OneTimeShutdown() {
	}

	void MangaSettingsView::OnOpen() {
		_CloseRequested = false; // We're only just starting
		CurViewState = eViewState::VIEWSTATE_OPEN;
	}
	void MangaSettingsView::OnClose() {
		_CloseRequested = true;
		//CurViewState = eViewState::VIEWSTATE_CLOSED;
		HideGUI();
	}

	void MangaSettingsView::UpdateMenuState(void) {
		
		if (_Fader.GetFadeState() != Fader::FADE_NONE) {
			Fader::eFadeState prevState = _Fader.GetFadeState();

			if (_Fader.GetFadeState() == Fader::FADE_IN && !_Menu->IsOpen()) {
				// We're fading in - show menu
				_Menu->Open();
			}

			_Fader.Update(3, Time::Delta);

			if (_Fader.GetFadeState() == Fader::FADE_NONE && prevState == Fader::FADE_OUT) {
				// We're done - close menu
				_Menu->Close();

				if (_CloseRequested) {
					CurViewState = eViewState::VIEWSTATE_CLOSED;
				}
			}

			// Update visual representation
			_CenterContainer->SetColor(Vector4f(_Fader.GetFadeAlpha()));
			_LeftContainer->SetColor(Vector4f(_Fader.GetFadeAlpha()));
			_RightContainer->SetColor(Vector4f(_Fader.GetFadeAlpha()));

			Vector3f pos = _CenterContainer->GetLocalPosition();
			_CenterContainer->SetLocalPosition(Vector3f(pos.x, (1 - _Fader.GetFadeAlpha()) * -0.05f, pos.z));
			
			pos = _LeftContainer->GetLocalPosition();
			_LeftContainer->SetLocalPosition(Vector3f(pos.x, (1 - _Fader.GetFadeAlpha()) * -0.05f, pos.z));

			pos = _RightContainer->GetLocalPosition();
			_RightContainer->SetLocalPosition(Vector3f(pos.x, (1 - _Fader.GetFadeAlpha()) * -0.05f, pos.z));
		}

		_OrientationContainer->SetLocalRotation(AppState::Conf->Orientation * MenuOffset);

	}

	void MangaSettingsView::ShowGUI(void) {
		//pose.Position = Vector3f::ZERO;
		CalcMenuOffset();
		// Update values
		if (_Mangaroll->CurrentManga != NULL) {
			_ProgressComponent.SetMax(_Mangaroll->CurrentManga->GetCount());
			_ProgressComponent.SetProgress(_Mangaroll->CurrentManga->GetProgress() / (_Mangaroll->CurrentManga->GetCount() - 1.0f));
			_PageLabel->SetText(String::Format(AppState::Strings->PageNo.ToCStr(), _Mangaroll->CurrentManga->GetProgress() + 1));

			String text = MangaStringUtils::CropToLength(_Mangaroll->CurrentManga->Name, 50);
			_TitleLabel->SetFontScale((1 - (text.GetLengthI() / 50.0f)) * 0.4f + 0.3f);
			_TitleLabel->SetTextWordWrapped(text.ToCStr(), _Mangaroll->GetGuiSys().GetDefaultFont(), PixelScale(_MainContainerWidth - 10));

			_TitleLabel->CalculateTextDimensions();

			_ShaderToggle->SetChecked(AppState::Conf->Transparent);
			_AutoToggle->SetChecked(AppState::Conf->AutoRotate);
			_ReadDirToggle->SetChecked(AppState::Conf->LeftToRight);
			_HelperToggle->SetChecked(AppState::Conf->Guided);

		}

		_Fader.StartFadeIn();
	}

	void MangaSettingsView::HideGUI(void) {
		_Fader.StartFadeOut();
	}


	// ########### STATIC EVENT HANDLERS ###############
	void MangaSettingsView::OnShaderChanged(UICheckbox *, void *p, bool transparent) {
		//MangaSettingsView *self = (MangaSettingsView *)p;

		AppState::Conf->Transparent = transparent;
	}

	void MangaSettingsView::OnReadDirChanged(UICheckbox *, void *p, bool leftToRight) {
		MangaSettingsView *self = (MangaSettingsView *)p;
		AppState::Conf->LeftToRight = leftToRight;
		
		self->_Mangaroll->Carousel.ChangeDirection();
	}
	void MangaSettingsView::OnAutoRotateChanged(UICheckbox *, void *p, bool autoRotate) {
		//MangaSettingsView *self = (MangaSettingsView *)p;
		AppState::Conf->AutoRotate = autoRotate;
	}
	void MangaSettingsView::OnGuideChanged(UICheckbox *, void *p, bool guide) {
		//MangaSettingsView *self = (MangaSettingsView *)p;
		AppState::Conf->Guided = guide;
	}

	// ##########################################


//##################################################################

	UIScrubBar::UIScrubBar(OvrGuiSys &gui, int width, int height) 
		: UIObject(gui)
		, _Width(width)
		, _Height(height)
		, _ProgressComponent()
		, _BGImage(NULL)
		, _FGImage(NULL)
	{
	}

	UIScrubBar::~UIScrubBar() {
		
	}

	void UIScrubBar::AddToMenu(UIMenu *menu, UIObject *parent) {
		const Posef pose(Quatf(Vector3f(0.0f, 1.0f, 0.0f), 0.0f), Vector3f(0.0f, 0.0f, 0.0f));

		Vector3f defaultScale(1.0f);
		VRMenuFontParms fontParms(HORIZONTAL_CENTER, VERTICAL_CENTER, false, false, false, 1.0f);

		VRMenuObjectParms parms(VRMENU_CONTAINER, Array< VRMenuComponent* >(), VRMenuSurfaceParms(),
			"", pose, defaultScale, fontParms, menu->AllocId(),
			VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t(VRMENUOBJECT_INIT_FORCE_POSITION));
		
		AddToMenuWithParms(menu, parent, parms);

		Init();
	}

	void UIScrubBar::Init(void) {
		_BGImage = new UIImage(GuiSys);
		_BGImage->AddToMenu(Menu, this);
		_BGImage->SetImage(0, eSurfaceTextureType::SURFACE_TEXTURE_DIFFUSE, Assets::Instance().ProgressBG, _Width, _Height);
		_BGImage->SetLocalPosition(Vector3f(0, -0.05f, 0));
		_BGImage->AddComponent(&_ProgressComponent);
		_BGImage->GetMenuObject()->AddFlags(VRMENUOBJECT_RENDER_HIERARCHY_ORDER);

		_FGImage = new UIImage(GuiSys);
		_FGImage->AddToMenu(Menu, _BGImage);
		_FGImage->SetLocalPosition(PixelPos(-3, 0, 1));
		_FGImage->SetImage(0, SURFACE_TEXTURE_ADDITIVE, Assets::Instance().Fill, 0, 0);
		_FGImage->GetMenuObject()->AddFlags(VRMenuObjectFlags_t(VRMENUOBJECT_DONT_HIT_ALL));

		_ProgressComponent.SetWidgets(Menu, _BGImage, _FGImage, NULL, NULL, _Width - 5, _Height - 5);
		_ProgressComponent.SetMax(100);
	}

	void UIScrubBar::SetFillColor(Vector4f col) {
		_FGImage->SetColor(col);
	}

	ScrubBarComponent &UIScrubBar::GetComponent(void) {
		return _ProgressComponent;
	}

	void UIScrubBar::SetIndicators(UILabel *valueIndicator, UILabel *seekIndicator) {
		_ProgressComponent.SetWidgets(Menu, _BGImage, _FGImage, valueIndicator, seekIndicator, _Width, _Height);
	}

	ScrubBarComponent::ScrubBarComponent() :
		VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_TOUCH_DOWN) |
		VRMENU_EVENT_TOUCH_DOWN |
		VRMENU_EVENT_FRAME_UPDATE |
		VRMENU_EVENT_FOCUS_GAINED |
		VRMENU_EVENT_FOCUS_LOST),
		HasFocus(false),
		TouchDown(false),
		Progress(0.0f),
		Max(0),
		Menu(NULL),
		Background(NULL),
		ScrubBar(NULL),
		CurrentTime(NULL),
		SeekTime(NULL),
		OnClickFunction(NULL),
		OnSetTextFunction(NULL),
		OnClickObject(NULL),
		OnTextObject(NULL)
	{
	}

	void ScrubBarComponent::SetMax(const int max)
	{
		Max = max;

		SetProgress(Progress);
	}

	int ScrubBarComponent::GetMax(void) {
		return Max;
	}

	void ScrubBarComponent::SetOnClick(void(*callback)(ScrubBarComponent *, void *, float), void *object)
	{
		OnClickFunction = callback;
		OnClickObject = object;
	}

	void ScrubBarComponent::SetOnText(void(*callback)(ScrubBarComponent *, void*, UILabel *label, int progress), void *object) {
		OnSetTextFunction = callback;
		OnTextObject = object;
	}

	void ScrubBarComponent::SetWidgets(UIMenu * menu, UIObject *background, UIObject *scrubBar, UILabel *currentTime, UILabel *seekTime, const int scrubBarWidth, const int scrubBarHeight)
	{
		Menu = menu;
		Background = background;
		ScrubBar = scrubBar;
		CurrentTime = currentTime;
		SeekTime = seekTime;
		ScrubBarWidth = scrubBarWidth;
		ScrubBarHeight = scrubBarHeight;

		if (SeekTime != NULL)
			SeekTime->SetVisible(false);
	}

	float ScrubBarComponent::GetProgress(void) {
		return Progress;
	}

	void ScrubBarComponent::SetProgress(const float progress)
	{
		Progress = progress;
		const float seekwidth = ScrubBarWidth * progress;

		Vector3f pos = ScrubBar->GetLocalPosition();
		pos.x = PixelScale((ScrubBarWidth - seekwidth) * -0.5f);
		ScrubBar->SetLocalPosition(pos);
		ScrubBar->SetSurfaceDims(0, Vector2f(seekwidth, ScrubBarHeight));
		ScrubBar->RegenerateSurfaceGeometry(0, false);

		/*pos = CurrentTime->GetLocalPosition();
		pos.x = PixelScale(ScrubBarWidth * -0.5f + seekwidth);
		CurrentTime->SetLocalPosition(pos);*/

		if (OnSetTextFunction != NULL && CurrentTime != NULL) {
			(OnSetTextFunction)(this, OnTextObject, CurrentTime, Max * progress);
		}
		//SetTimeText(CurrentTime, Max * progress);
	}

	//void ScrubBarComponent::SetTimeText(UILabel *label, const int time)
	//{
	//	int seconds = time / 1000;
	//	int minutes = seconds / 60;
	//	int hours = minutes / 60;
	//	seconds = seconds % 60;
	//	minutes = minutes % 60;

	//	if (hours > 0)
	//	{
	//		label->SetText(StringUtils::Va("%d:%02d:%02d", hours, minutes, seconds));
	//	}
	//	else if (minutes > 0)
	//	{
	//		label->SetText(StringUtils::Va("%d:%02d", minutes, seconds));
	//	}
	//	else
	//	{
	//		label->SetText(StringUtils::Va("0:%02d", seconds));
	//	}
	//}

	eMsgStatus ScrubBarComponent::OnEvent_Impl(OvrGuiSys & guiSys, VrFrame const & vrFrame,
		VRMenuObject * self, VRMenuEvent const & event)
	{
		switch (event.EventType)
		{
		case VRMENU_EVENT_FOCUS_GAINED:
			HasFocus = true;
			return MSG_STATUS_ALIVE;

		case VRMENU_EVENT_FOCUS_LOST:
			HasFocus = false;
			return MSG_STATUS_ALIVE;

		case VRMENU_EVENT_TOUCH_DOWN:
			TouchDown = true;
			OnClick(guiSys, vrFrame, event);
			AppState::Reader->LongPressInterrupted = true;
			return MSG_STATUS_ALIVE;

		case VRMENU_EVENT_FRAME_UPDATE:
			return OnFrame(guiSys, vrFrame, self, event);

		default:
			OVR_ASSERT(!"Event flags mismatch!");
			return MSG_STATUS_ALIVE;
		}
	}

	eMsgStatus ScrubBarComponent::OnFrame(OvrGuiSys & guiSys, VrFrame const & vrFrame,
		VRMenuObject * self, VRMenuEvent const & event)
	{
		if (TouchDown)
		{
			if ((vrFrame.Input.buttonState & (BUTTON_A | BUTTON_TOUCH)) != 0)
			{
				OnClick(guiSys, vrFrame, event);
			}
			else
			{
				TouchDown = false;
			}
		}

		if (SeekTime != nullptr)
			SeekTime->SetVisible(HasFocus);
		if (HasFocus)
		{
			guiSys.GetGazeCursor().ForceDistance(event.HitResult.t, CURSOR_STATE_HILIGHT);

			Vector3f hitPos = event.HitResult.RayStart + event.HitResult.RayDir * event.HitResult.t;

			// move hit position into local space
			const Posef modelPose = Menu->GetVRMenu()->GetMenuPose() * Background->GetWorldPose();
			Vector3f localHit = modelPose.Orientation.Inverted().Rotate(hitPos - modelPose.Position);

			//WARN("%.2f | %.2f | %.2f", modelPose.Orientation.x, modelPose.Orientation.y, modelPose.Orientation.z);
			Bounds3f bounds = Background->GetMenuObject()->GetLocalBounds(guiSys.GetDefaultFont()) * Background->GetParent()->GetWorldScale();
			const float progress = (localHit.x - bounds.GetMins().x) / bounds.GetSize().x;

			if ((progress >= 0.0f) && (progress <= 1.0f))
			{

				if (SeekTime != NULL) {
					const float seekwidth = ScrubBarWidth * progress;

					Vector3f pos = SeekTime->GetLocalPosition();
					pos.x = PixelScale(ScrubBarWidth * -0.5f + seekwidth);
					SeekTime->SetLocalPosition(pos);

					if (OnSetTextFunction != NULL) {
						(OnSetTextFunction)(this, OnTextObject, SeekTime, Max * progress);
					}
				}
				//SetTimeText(SeekTime, Max * progress);
			}
		}

		return MSG_STATUS_ALIVE;
	}

	void ScrubBarComponent::OnClick(OvrGuiSys & guiSys, VrFrame const & vrFrame, VRMenuEvent const & event)
	{
		if (OnClickFunction == NULL)
		{
			return;
		}

		Vector3f hitPos = event.HitResult.RayStart + event.HitResult.RayDir * event.HitResult.t;

		// move hit position into local space
		const Posef modelPose = Menu->GetVRMenu()->GetMenuPose() * Background->GetWorldPose();
		Vector3f localHit = modelPose.Orientation.Inverted().Rotate(hitPos - modelPose.Position);

		Bounds3f bounds = Background->GetMenuObject()->GetLocalBounds(guiSys.GetDefaultFont()) * Background->GetParent()->GetWorldScale();
		const float progress = (localHit.x - bounds.GetMins().x) / bounds.GetSize().x;
		if ((progress >= 0.0f) && (progress <= 1.0f))
		{
			(*OnClickFunction)(this, OnClickObject, progress);
		}
	}



}