/*
 * Copyright (c) 2010-2016 Stephane Poirier
 *
 * stephane.poirier@oifii.org
 *
 * Stephane Poirier
 * 3532 rue Ste-Famille, #3
 * Montreal, QC, H2X 2L1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

////////////////////////////////////////////////////////////////
//nakedsoftware.org, spi@oifii.org or stephane.poirier@oifii.org
//
//
//2013nov13, creation for constructing rhythm
//
//
//nakedsoftware.org, spi@oifii.org or stephane.poirier@oifii.org
////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include <string>
#include <fstream>
#include <vector>

#include <iostream>
#include <sstream>
using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include "portaudio.h"

#define BUFF_SIZE	2048


#include <ctime>
#include "spiws_WavSet.h"
#include "spiws_Instrument.h"
#include "spiws_InstrumentSet.h"

#include "spiws_partition.h"
#include "spiws_partitionset.h"

#include "spiws_WavSet.h"

#include <assert.h>
#include <windows.h>



// Select sample format
#if 1
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 1
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)
#define PRINTF_S_FORMAT "%d"
#endif

//The event signaled when the app should be terminated.
HANDLE g_hTerminateEvent = NULL;
//Handles events that would normally terminate a console application. 
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType);

int Terminate();

InstrumentSet* pInstrumentSet=NULL;


//////////////////////////////////////////
//main
//////////////////////////////////////////
int main(int argc, char *argv[]);
int main(int argc, char *argv[])
{
	int nShowCmd = false;
	ShellExecuteA(NULL, "open", "begin.bat", "", NULL, nShowCmd);

	///////////////////
	//read in arguments
	///////////////////
	char charBuffer[2048] = {"."}; //default folder
	//char charBuffer[2048] = {"JAVADRUM 3 (Wave 1).WAV"};
	//float fSecondsPlay = -1.0; //negative for playing only once
	float fSecondsPlay = 300; //positive for number of seconds to play/loop
	//double fSecondsPerSegment = 1.0; //non-zero for spliting sample into sub-segments
	//double fSecondsPerSegment = 0.0625; //non-zero for spliting sample into sub-segments
	double fSecondsPerSegment = 0.010; //non-zero for spliting sample into sub-segments
	if(argc>1)
	{
		//first argument is the filename
		sprintf_s(charBuffer,2048-1,argv[1]);
	}
	if(argc>2)
	{
		//second argument is the time it will play
		fSecondsPlay = atof(argv[2]);
	}
	if(argc>3)
	{
		//third argument is the segment length in seconds
		fSecondsPerSegment = atof(argv[3]);
	}
	//use audio_spi\spidevicesselect.exe to find the name of your devices, only exact name will be matched (name as detected by spidevicesselect.exe)  
	//string audiodevicename="E-MU ASIO"; //"Speakers (2- E-MU E-DSP Audio Processor (WDM))"
	string audiodevicename="Speakers (2- E-MU E-DSP Audio P"; //"E-MU ASIO"
	if(argc>4)
	{
		audiodevicename = argv[4]; //for spi, device name could be "E-MU ASIO", "Speakers (2- E-MU E-DSP Audio Processor (WDM))", etc.
	}
    int outputAudioChannelSelectors[2]; //int outputChannelSelectors[1];
	/*
	outputAudioChannelSelectors[0] = 0; // on emu patchmix ASIO device channel 1 (left)
	outputAudioChannelSelectors[1] = 1; // on emu patchmix ASIO device channel 2 (right)
	*/
	/*
	outputAudioChannelSelectors[0] = 2; // on emu patchmix ASIO device channel 3 (left)
	outputAudioChannelSelectors[1] = 3; // on emu patchmix ASIO device channel 4 (right)
	*/
	outputAudioChannelSelectors[0] = 6; // on emu patchmix ASIO device channel 15 (left)
	outputAudioChannelSelectors[1] = 7; // on emu patchmix ASIO device channel 16 (right)
	if(argc>5)
	{
		outputAudioChannelSelectors[0]=atoi(argv[5]); //0 for first asio channel (left) or 2, 4, 6 and 8 for spi (maxed out at 10 asio output channel)
	}
	if(argc>6)
	{
		outputAudioChannelSelectors[1]=atoi(argv[6]); //1 for second asio channel (right) or 3, 5, 7 and 9 for spi (maxed out at 10 asio output channel)
	}

    //Auto-reset, initially non-signaled event 
    g_hTerminateEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    //Add the break handler
    ::SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

	PaStreamParameters outputParameters;
    PaStream* stream;
    PaError err;

	////////////////////////
	// initialize port audio 
	////////////////////////
    err = Pa_Initialize();
    if( err != paNoError )
	{
		fprintf(stderr,"Error: Initialization failed.\n");
		Pa_Terminate();
		fprintf( stderr, "An error occured while using the portaudio stream\n" );
		fprintf( stderr, "Error number: %d\n", err );
		fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
		return -1;
	}

	outputParameters.device = Pa_GetDefaultOutputDevice(); // default output device 
	if (outputParameters.device == paNoDevice) 
	{
		fprintf(stderr,"Error: No default output device.\n");
		Pa_Terminate();
		fprintf( stderr, "An error occured while using the portaudio stream\n" );
		fprintf( stderr, "Error number: %d\n", err );
		fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
		return -1;
	}
	outputParameters.channelCount = 2;//pWavSet->numChannels;
	outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;


	//////////////////////////
	//initialize random number
	//////////////////////////
	srand((unsigned)time(0));



	/////////////////////////////
	//loop n sinusoidal samples
	/////////////////////////////
	WavSet* pTempWavSet = new WavSet;
	pTempWavSet->CreateSin(0.3333, 44100, 2, 440.0, 0.5f);
	WavSet* pSilentWavSet = new WavSet;
	pSilentWavSet->CreateSilence(5);
	pSilentWavSet->LoopSample(pTempWavSet, 5, -1.0, 0.0); //from second 0, loop sample during 5 seconds
	pSilentWavSet->Play(&outputParameters);
	if(pTempWavSet)
	{
		delete pTempWavSet;
		pTempWavSet = NULL;
	}
	if(pSilentWavSet)
	{
		delete pSilentWavSet;
		pSilentWavSet = NULL;
	}
	

	

	//////////////////////////////////////////////////////////////////////////
	//populate InstrumentSet according to input folder (folder of sound files)
	//////////////////////////////////////////////////////////////////////////
	pInstrumentSet=new InstrumentSet;
	if(pInstrumentSet!=NULL)
	{
		pInstrumentSet->Populate(charBuffer, 0);
	}
	else
	{
		assert(false);
		cout << "exiting, instrumentset could not be allocated" << endl;
		Pa_Terminate();
		return -1;
	}
		


	////////////////////////////////
	//play each wavfile in wavfolder
	////////////////////////////////
	Instrument* pAnInstrument = NULL;
	WavSet* pAWavSet;
	if(pInstrumentSet && pInstrumentSet->HasOneInstrument()) 
	{
		for(int ii=0; ii<pInstrumentSet->GetNumberOfInstrument(); ii++)
		{
			pAnInstrument = pInstrumentSet->GetInstrumentFromID(ii);
			assert(pAnInstrument);
			cout << "instrument name: " << pAnInstrument->instrumentname << endl;
			for(int jj=0; jj<pAnInstrument->GetNumberOfWavSet(); jj++)
			{
				pAWavSet = pAnInstrument->GetWavSetFromID(jj); 
				assert(pAWavSet);
				cout << "sound filename: " << pAWavSet->GetName() << endl;
				pAWavSet->Play(USING_SPIPLAY, 300);
			}
		}
	}



	Terminate();
	return 0;
}

int Terminate()
{
	/////////////////////
	//terminate portaudio
	/////////////////////
	Pa_Terminate();

	//delete all memory allocations
	if(pInstrumentSet!=NULL) delete pInstrumentSet;

	printf("Exiting!\n"); fflush(stdout);

	int nShowCmd = false;
	ShellExecuteA(NULL, "open", "end.bat", "", NULL, nShowCmd);
	return 0;
}

//Called by the operating system in a separate thread to handle an app-terminating event. 
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
    if (dwCtrlType == CTRL_C_EVENT ||
        dwCtrlType == CTRL_BREAK_EVENT ||
        dwCtrlType == CTRL_CLOSE_EVENT)
    {
        // CTRL_C_EVENT - Ctrl+C was pressed 
        // CTRL_BREAK_EVENT - Ctrl+Break was pressed 
        // CTRL_CLOSE_EVENT - Console window was closed 
		Terminate();
        // Tell the main thread to exit the app 
        ::SetEvent(g_hTerminateEvent);
        return TRUE;
    }

    //Not an event handled by this function.
    //The only events that should be able to
	//reach this line of code are events that
    //should only be sent to services. 
    return FALSE;
}

