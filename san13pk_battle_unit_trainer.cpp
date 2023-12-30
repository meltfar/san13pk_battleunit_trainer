// san13pk_battle_unit_trainer.cpp : Defines the entry point for the application.
//

#include "sciter-x.h"
#include "sciter-x-window.hpp"
#include "san13pk_battle_unit_trainer.h"

#include "engine.h"
#include <string>


#include "resources.cpp"

class MainFrame : public sciter::window {
public:
	MainFrame() : window(SW_MAIN | SW_TITLEBAR | SW_RESIZEABLE | SW_CONTROLS | SW_ENABLE_DEBUG) {

	}

	SOM_PASSPORT_BEGIN(MainFrame)
		SOM_FUNCS(
			SOM_FUNC(nativeMessage),
			SOM_FUNC(initializeTrainer),
			SOM_FUNC(getPlayerIndex),
			SOM_FUNC(enhancePlayer),
		)
	SOM_PASSPORT_END

	// function expsed to script:
	sciter::string  nativeMessage() { return WSTR("Hello C++ World"); }

	bool initializeTrainer() {
		return gameEngine.searchAndOpen(L"San13PK_sc.exe");
	}

	int getPlayerIndex() {
		return gameEngine.getPlayerIndex();
	}

	// function to call engine.enhancePlayerWith
	bool enhancePlayer(int troopType, bool ladderNotAllowed, bool keepRation, bool keepMorale, int troopsNumber, bool noInjuries) {
		// this->call_function("consoleLog", troopType);
		return gameEngine.enhancePlayerWith(static_cast<byte>(troopType), !ladderNotAllowed, keepRation, keepMorale, troopsNumber, noInjuries);
	}
protected:
	Engine gameEngine;
};

int uimain(std::function<int()> run) {
	SciterSetOption(NULL, SCITER_SET_SCRIPT_RUNTIME_FEATURES,
		ALLOW_FILE_IO |
		ALLOW_SOCKET_IO |
		ALLOW_EVAL |
		ALLOW_SYSINFO);

	sciter::archive::instance().open(aux::elements_of(resources));
#ifdef _DEBUG
	sciter::debug_output_console console;
#endif // DEBUG

	sciter::om::hasset<MainFrame> mainFrame = new MainFrame();

	mainFrame->load(WSTR("this://app/index.html"));
	//mainFrame->load(WSTR("file:D:/projects/cpp/san13pk_battle_unit_trainer/res/index.html"));

	mainFrame->expand();

	return run();
}