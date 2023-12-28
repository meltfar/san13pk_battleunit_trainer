#pragma once
#include "Windows.h"
#include "string"
#include <memory>
#include <tlhelp32.h>
#include <vector>
#include <stdexcept>
#include "consts.h"

#pragma pack(1)
struct BattleUnit
{
	/*
	* 00 00 编号
68 00 军势号
22 00 所属城
62 02 主将
FF FF 副将1
FF FF 副将2
85 00 军粮
B3 15 士兵数量
03 00 伤兵数
E8 1C 士气
09 兵种
12 兵器
16 船只
00 不知道有啥用
FF FF FF FF 
FF FF FF FF 
FF FF FF FF 
FF FF FF FF 
FF FF FF FF 
00 00 开始战斗时间？
00 00 有啥用？
E8 46 20 01 一个指针，貌似指向父类
	*/
	WORD index;
	WORD partyIndex;
	WORD fromCityIndex;
	WORD leaderIndex;
	WORD deputyIndex1;
	WORD deputyIndex2;
	WORD rationsRemain;
	WORD troopCount;
	WORD injuredCount;
	WORD morale;
	byte troopType;
	byte weaponType;
	byte shipType;
	byte reserved1;
	byte reserved2[20];
	WORD startFrom;
	WORD reserved3;
	DWORD pointerToBase;
};
#pragma pack()

DWORD findProcessByName(const std::wstring &gameName) {
	PROCESSENTRY32 entry{};
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (gameName == entry.szExeFile)
			{
				// HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
				CloseHandle(snapshot);
				return entry.th32ProcessID;
				// CloseHandle(hProcess);
			}
		}
	}

	CloseHandle(snapshot);
	return 0;
}

uintptr_t GetModuleBaseAddress(DWORD procId, const std::wstring &modName)
{
	uintptr_t modBaseAddr = 0;

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, procId);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 modEntry{};
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry))
		{
			do
			{
				if (modEntry.szModule == modName)
				{
					modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}
	CloseHandle(hSnap);
	return modBaseAddr;
}

class Engine {
	HANDLE gameHandle = NULL;
	BattleUnit troopInfos[MAXMIUM_TROOPS];
	WORD playerIndex = 0;
	uintptr_t moduleBaseAddress;

public:
	bool searchAndOpen(const std::wstring& gameName) {
		auto processId = findProcessByName(gameName);

		if (processId <= 0) {
			MessageBox(nullptr, L"failed to find game process", L"Error", MB_ICONERROR);
			return false;
		}

		this->gameHandle = OpenProcess(PROCESS_ALL_ACCESS, false, processId);
		if (this->gameHandle == INVALID_HANDLE_VALUE) {
			MessageBox(nullptr, L"failed to open game process", L"Error", MB_ICONERROR);
			return false;
		}

		auto moduleBaseAddress = GetModuleBaseAddress(processId, gameName);
		if (moduleBaseAddress == (uintptr_t)0) {
			MessageBox(nullptr, L"failed to get base address of module", L"Error", MB_ICONERROR);
		}
		this->moduleBaseAddress = moduleBaseAddress;

		return true;
	}

	bool stillValid() const {
		if (this->gameHandle != NULL && this->gameHandle != INVALID_HANDLE_VALUE) {
			return true;
		}
		return false;
	}

	void checkValid() const {
		if (!stillValid()) {
			throw std::runtime_error("engine has not initialized properly or game already exited.");
		}
	}

	std::vector<byte> readData(const DWORD destAddress, const size_t length) {
		auto ret = std::vector<byte>(length);

		if (ReadProcessMemory(this->gameHandle, reinterpret_cast<LPCVOID>(destAddress), ret.data(), length, nullptr)) {
			return ret;
		} else {
			return {};
		}
	}

	bool writeData(const DWORD destAddress, const byte* buffer, const size_t length) {
		return WriteProcessMemory(this->gameHandle, reinterpret_cast<LPVOID>(destAddress), buffer, length, nullptr);
	}

	void updateTroopsInfo() {
		checkValid();
		auto troopInfos = this->readData(this->moduleBaseAddress + TROOPS_BASE_ADDRESS_OFFSET, sizeof(BattleUnit) * MAXMIUM_TROOPS);

		if (troopInfos.size() <= 0) {
			throw std::runtime_error("failed to get battle unit information: " + GetLastError());
		}

		memcpy(&this->troopInfos, troopInfos.data(), troopInfos.size());
		//this->troopInfos = std::vector<BattleUnit>(troopInfos.data(), troopInfos.data() + troopInfos.size());

		//if (this->troopInfos.size() != MAXMIUM_TROOPS) {
		//	throw std::runtime_error("failed to verify incoming vector");
		//}
	}

	void updatePlayerIndex() {
		checkValid();
		auto playerBytes = readData(this->moduleBaseAddress + PLAYER_ADDRESS_OFFSET, 2);
		if (playerBytes.size() <= 0) {
			throw std::runtime_error("failed to get player id information: " + GetLastError());
		}

		WORD playerIndex = *reinterpret_cast<WORD*>(playerBytes.data());
		if (playerIndex < 0) {
			throw std::runtime_error("invalid player index");
		}

		this->playerIndex = playerIndex;
	}

	WORD getPlayerIndex() const {
		return this->playerIndex;
	}

	// 最外层的函数，做所有的事情
	bool enhancePlayerWith(const byte unitType, const bool ladderAllowed, const bool keepRationFull, const bool keepMorale, const DWORD keepTroops) {
		bool playerFound = false;
		try {
			// update all information
			this->updatePlayerIndex();
			this->updateTroopsInfo();

			// game has not been loaded.
			if (this->playerIndex == 0xFFFF) {
				return true;
			}

			for (auto& bu : this->troopInfos) {
				if (bu.leaderIndex == 0xFFFF) {
					continue;
				}
				if (bu.leaderIndex == this->playerIndex || bu.deputyIndex1 == this->playerIndex || bu.deputyIndex2 == this->playerIndex) {
					// detected player's unit
					playerFound = true;

					// deal with ladder
					if (!ladderAllowed) {
						bu.weaponType = 18; // index 18 : ladder
					}

					// deal with troop type, 9 - 弓骑兵，34 - 氐羌兵
					if (unitType < 9 || unitType > 34) {
						bu.troopType = 30; // 30 象兵
					} else {
						bu.troopType = unitType;
					}
					
					// deal with morale
					if (keepMorale) {
						bu.morale = 20000;
					}

					// deal with ration
					if (keepRationFull) {
						bu.rationsRemain = 200;
					}

					// deal with troops
					if (keepTroops > 0) {
						bu.troopCount = keepTroops;
					} else {
						bu.troopCount += bu.injuredCount;
					}
					bu.injuredCount = 0;
				} else if (!ladderAllowed) {
					// not player, but ladder is not allowed
					if (bu.weaponType == 18) {
						bu.weaponType = 20; // 18 - Ladder, 20 - jinglan
					}
				}
			}

			if (!playerFound && ladderAllowed) {
				return true;
			}

			// commit changes
			if (!this->writeData(this->moduleBaseAddress + TROOPS_BASE_ADDRESS_OFFSET, reinterpret_cast<byte*>(&this->troopInfos), sizeof(this->troopInfos))) {
				return false;
			}

			// finish
			return true;

		} catch (const std::exception& e) {
			std::cout << e.what() << std::endl;
			return false;
		}
	}
};