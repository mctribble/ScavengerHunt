//Scavenger Hunt: simple game that shows random images from the objectSamples folder 
//the goal is to provide the game with another image containing that object or showing the same location

#include <iostream>
#include <fstream>
#include <Windows.h>
#include <ShObjIdl.h>
#include "opencv2/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

using namespace std;
using namespace cv;

RNG rng(GetTickCount()); //declared global to make sure it only gets seeded once

int main()
{
	//standard behavior for applications is to return 0 on success and other values for anything else.  enum is used for organizational purposes
	enum ExitCodes
	{
		SUCCESS = 0,
		CANT_FIND_SAMPLES = 1,
		INVALID_ROUND_COUNT = 2,
		OPEN_FILE_DIALOG_ERROR = 3
	};
	int exitCode = ExitCodes::SUCCESS;

	//find all files in /objectSamples
	vector <string> sampleFiles;
	WIN32_FIND_DATAA curFileData;

	//init file search
	HANDLE curFileHandle = FindFirstFileA("./objectSamples/*", &curFileData); //get the first file
	if (curFileHandle == INVALID_HANDLE_VALUE)
	{
		wcerr << L"failed to find first sample files";
		return ExitCodes::CANT_FIND_SAMPLES;
	}

	do {
		if (!(curFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) //if this file is NOT a directory
			sampleFiles.push_back(curFileData.cFileName);	//put its file name in the list
	} while (FindNextFileA(curFileHandle, &curFileData) != 0); //keep going until we run out of files
	FindClose(curFileHandle);

	//report sample count and ask user how many rounds they want to play
	cout << "found " << sampleFiles.size() << " sample images.  Each round, one of these is chosen at random.  How many rounds do you wish to play?" << endl;
	int gameRoundCount = -1;
	cin >> gameRoundCount;
	if ((gameRoundCount < 1) || (gameRoundCount > sampleFiles.size()))
	{
		cerr << "invalid round count." << endl;
		return ExitCodes::INVALID_ROUND_COUNT;
	}
	
	//shuffle the file list by randomly swapping items in the list around
	int SHUFFLE_SWAP_MULT = 3; //perform SHUFFLE_SWAP_MULT times as many swaps as there are items in the list
	for (int i = 0; i < (sampleFiles.size()*SHUFFLE_SWAP_MULT); i++)
	{
		int a = rng.uniform(0, (int)sampleFiles.size());
		int b = rng.uniform(0, (int)sampleFiles.size());
		string temp = sampleFiles[a];
		sampleFiles[a] = sampleFiles[b];
		sampleFiles[b] = temp;
	}

	//game instructions
	cout << "Each round, you will be presented with an image." << endl <<
		"Step 1: Try to find another image containg the same object or showing the same place and save it somewhere on your computer" << endl <<
		"Step 2: Press any key to open the file dialog and navigate to the file" << endl <<
		"Step 3: wait a few seconds and you will be told whether or not a match was found and scored accordingly" << endl <<
		"(if no match was found, you are given two more attempts without penalty before it is counted as a miss)" << endl << endl;

	//score tracking
	unsigned int matchCount = 0;
	unsigned int missCount = 0;

	//main game loop
	for (int curRound = 0; curRound < gameRoundCount; curRound++)
	{
		//report round number
		cout << "Round " << (curRound + 1) << "/" << gameRoundCount << ": " << sampleFiles[curRound].c_str();

		//load it, and present it to the user
		string sampleName = "objectSamples/" + sampleFiles[curRound];
		Mat sampleImageUser = imread( sampleName, IMREAD_REDUCED_COLOR_4);	//sample image shown to the user
		Mat sampleImageAlgo = imread( sampleName, IMREAD_GRAYSCALE); //sample image used for the image matching
		imshow("Find this", sampleImageUser);	//show the image

		//wait for a couple seconds before demanding a response 
		//(also allows openCV to catch window events: commenting this breaks EVERYTHING, but it can be safely dropped to 1 ms)
		waitKey(2000);
		
		byte strikes = 0;
		bool matchFound = false;
		for (; (strikes < 3) && (matchFound == false); strikes++) //main guess loop
		{
			//prompt user for a file to test against (https://msdn.microsoft.com/en-us/library/windows/desktop/ff485843%28v=vs.85%29.aspx)
			HRESULT result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); //initialize COM lib
			if (FAILED(result))
				return ExitCodes::OPEN_FILE_DIALOG_ERROR;

			//make a file dialog to present to the user
			IFileOpenDialog* dialog;
			result = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&dialog));
			if (FAILED(result))
			{
				CoUninitialize();
				return ExitCodes::OPEN_FILE_DIALOG_ERROR;
			}


			//present it
			result = dialog->Show(NULL);
			if (FAILED(result))
			{
				//errors here tend to result from the user cancelling the dialog, so we just treat it as a miss instead of dying
				dialog->Release();
				CoUninitialize();
				cout << "[X]"; 
				continue;
			}

			//get the result
			IShellItem* shellItem;
			result = dialog->GetResult(&shellItem);
			if (FAILED(result))
			{
				dialog->Release();
				CoUninitialize();
				return ExitCodes::OPEN_FILE_DIALOG_ERROR;
			}

			//get the file name from it
			wstring fileName;
			PWSTR fileNameTemp;	//temporary buffer for the file name
			result = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &fileNameTemp); //fetch the file name
			shellItem->Release();	//we dont need this anymore
			dialog->Release();		//or this
			if (FAILED(result))
			{
				CoUninitialize();
				return ExitCodes::OPEN_FILE_DIALOG_ERROR;
			}
			else
			{
				//copy data from the temporary buffer to where we actually want it
				fileName = fileNameTemp;

				CoTaskMemFree(fileNameTemp); //clean up the buffer
				CoUninitialize(); //clean up the COM lib
			}

			//perform the test
			wstring args = L"objectDetection \"" + wstring(sampleName.begin(), sampleName.end()) + L"\" \"" + fileName + L"\" lastMatch.png";
			SHELLEXECUTEINFO ShExecInfo = { 0 };
			ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
			ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
			ShExecInfo.hwnd = NULL;
			ShExecInfo.lpVerb = NULL;
			ShExecInfo.lpFile = L"ScreenSearch.exe";
			ShExecInfo.lpParameters = (args.c_str());
			ShExecInfo.lpDirectory = NULL;
			ShExecInfo.nShow = SW_HIDE;
			ShExecInfo.hInstApp = NULL;
			ShellExecuteEx(&ShExecInfo);

			//wait for screenSearch to finish, with a '.' every second to indicate it is still running
			DWORD waitResult = WAIT_TIMEOUT;
			while (waitResult == WAIT_TIMEOUT)
			{
				cout << '.';
				waitResult = WaitForSingleObject(ShExecInfo.hProcess, 1000);
			}
			
			DWORD matchResult = -1;
			GetExitCodeProcess(ShExecInfo.hProcess, &matchResult);

			switch (matchResult)
			{
			case 0:
				//there was a successful match!
				cout << "[MATCH]";
				matchCount++;
				matchFound = true;
				break;
			case 1:
				//there was no match
				cout << "[X]";
				waitKey(500); //wait just a moment so user is more likely to see the [X]
				missCount++;
				break;
			case 2:
				//we passed invalid arguments to screensearch. 
				cout << "[BAD ARGS!]";
				throw invalid_argument("Invalid arguments sent to screensearch!");
				break;
			default:
				//because all matches are given return code 0, we can assume unknown errors, whatever they are, were not a match.  
				//Still, there should be better handling of this.  Pause to make sure the user sees it then treat it as a regular miss.
				cerr << "UNKNOWN RETURN VALUE!";
				cin.clear();
				cin.get();
				missCount++;
				break;
			}
		} //end guess loop
		cout << endl;

	} //end main game loop

	//destroy the image window since we no longer need it
	destroyWindow("Find this");

	//Scoring constants
	float	ACCURACY_MAX_VALUE = 1000.0f;	//proportional to %accuracy
	int		MATCH_VALUE = 100;				//proportional to successful matches
	int		BASE_MISS_PENALTY = 20;			//quadratically proportional to missed matches

	//calculate score
	int score = 0;
	cout << "Game over!" << endl;

	//base score proportional to accuracy as that is the primary goal
	float accuracy = ((float)matchCount / (float)gameRoundCount)*100.0f;
	int accuracyBonus = (int)( ceil (accuracy * ACCURACY_MAX_VALUE) );
	cout << "Accuracy: " << accuracy << "% accurate (" << matchCount << "/" << gameRoundCount << "): " << accuracyBonus << " points" << endl;
	score += accuracyBonus;

	//TODO: time bonus?

	//score bonus for successful matches.  Linear growth so that games with more matches are worth more points
	int matchBonus = matchCount * MATCH_VALUE;
	cout << "Successful matches: " << matchCount << " X " << MATCH_VALUE << " = " << matchBonus << " points" << endl;
	score += matchBonus;

	//image matching is flaky, so we want to be somewhat generous with miss penalties but still heavily penalize guessing
	//to do this, the miss penalty grows quadratically: it is based on misses squared instead of the miss count directly
	int missCountSquared = missCount * missCount;
	int missPenalty = missCountSquared * BASE_MISS_PENALTY;
	cout << "Failed matches: " << missCountSquared << "^2 * " << BASE_MISS_PENALTY << " = -" << missPenalty << " points" << endl;
	score -= missPenalty;

	//final results
	cout << endl << endl << "Final Score: " << score << endl;

	//prompt for keypress before quitting
	cout << endl << "Press enter to exit." << endl;
	cin.ignore(); //make sure not to accept keypresses that happened much earlier
	cin.get();

    return exitCode;
}

