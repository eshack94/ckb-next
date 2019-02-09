/*
 * Copyright (C) 2017  Devon Richards
 * Copyright (C) 2019  Tasos Sahanidis <code@tasossah.com>
 * mviz is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * mviz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with mviz.  If not, see <http://www.gnu.org/licenses/>.
 */

#define USE_PORTAUDIO 1
#define DEBUG 1

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef USE_PORTAUDIO
#include <portaudio.h>
PaStream *stream = NULL;
#else
#include <pulse/simple.h>
pa_simple *pas = NULL;
#endif

#include <kissfft/kiss_fftr.h>
#include <ckb-next/animation.h>

#define RANGE 768 // ~15KHz
//#define RANGE (256 + 128)

void ckb_info(){
    // Plugin info
    CKB_NAME("Music Visualization");
    CKB_VERSION("0.3");
    CKB_COPYRIGHT("2017-2019", "RULER501 & TasosSah");
    CKB_LICENSE("GPLv2+");
    CKB_GUID("{097D69F0-70B2-48B8-AFE2-25CA1DB0D92C}");
    CKB_DESCRIPTION("A collection of music visualization effects");

    // Effect parameters
    CKB_PARAM_AGRADIENT("color", "Fade color:", "", "ffffffff");
	CKB_PARAM_BOOL("power", "Use Power instead of Magnitude?", 0);

    // Timing/input parameters
    CKB_KPMODE(CKB_KP_NONE);
    CKB_TIMEMODE(CKB_TIME_ABSOLUTE);
    CKB_REPEAT(FALSE);
    CKB_LIVEPARAMS(TRUE);
    
    // Presets
    CKB_PRESET_START("Default");
    CKB_PRESET_PARAM("power", "0");
	CKB_PRESET_PARAM("trigger", "0");
    CKB_PRESET_PARAM("kptrigger", "1");
    CKB_PRESET_END;
}

double powers[1024] = { 0.0 };
double hann_res[2048] = { 0.0 };
kiss_fft_cpx* inbuf;
kiss_fft_cpx* outbuf;
ckb_gradient animcolor = { 0 };
int power = 0;
int* translated_frame;
//double* key_stamp;

void ckb_init(ckb_runctx* context){
#ifdef USE_PORTAUDIO
    putenv("PULSE_LATENCY_MSEC=90");

    PaStreamParameters inputParams;
    PaError err;
    const PaDeviceInfo* inputInfo;
    err = Pa_Initialize();
    if(err != paNoError){
        fprintf(stderr, "Error initialising PortAudio %d\n", err);
        // TODO: Handle this somehow
    }
    
    inputParams.device = Pa_GetDefaultInputDevice();
    DBG("Dev id %d", inputParams.device);
    inputInfo = Pa_GetDeviceInfo(inputParams.device);
    DBG("Recording from %s", inputInfo->name);
    
    inputParams.channelCount = 1;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency = inputInfo->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = NULL;
    
    err = Pa_OpenStream(&stream, &inputParams, NULL, 44100, 512, paNoFlag, NULL, NULL );
    if(err != paNoError)
        fprintf(stderr, "Error opening PortAudio stream %d\n", err);
    
    err = Pa_StartStream(stream);
    if(err != paNoError)
        fprintf(stderr, "Error starting PortAudio stream %d\n", err);    
#else
	static const pa_sample_spec ss ={
		.format = PA_SAMPLE_S16LE,
		.rate = 44100,
		.channels = 1
	};
	pas = pa_simple_new(NULL, "CKB Music Viz", PA_STREAM_RECORD, NULL, "CKB Music Viz", &ss, NULL, NULL, NULL);
#endif

	inbuf = malloc(2048*sizeof(kiss_fft_cpx));
	outbuf = malloc(2048*sizeof(kiss_fft_cpx));
    translated_frame = malloc(RANGE * sizeof(int));
    //key_stamp = malloc(context->keycount * sizeof(double));
    
    for (int i = 0; i < 2048; i++)
        hann_res[i] = (1 - cos(2 * M_PI * i / 2048)) * 0.5;
}

void ckb_parameter(ckb_runctx* context, const char* name, const char* value){
    CKB_PARSE_AGRADIENT("color", &animcolor){};
    CKB_PARSE_BOOL("power", &power){};
}

void anim_add(ckb_key* press, float x, float y){
	return;
}

void anim_remove(float x, float y){
	return;
}

void ckb_keypress(ckb_runctx* context, ckb_key* key, int x, int y, int state){
	return;
}

void ckb_start(ckb_runctx* context, int state){
	return;
}

/*int flip = 1;
double deltatmr = 0.0;*/
void ckb_time(ckb_runctx* context, double delta){
   /* deltatmr += delta;
    if(deltatmr > 0.01)
    {
        flip = 1;
        deltatmr = 0.0;
    }
    DBG("%lf", delta);*/
    /*ckb_key* keys = context->keys;
	ckb_key* maxkey = keys+context->keycount-1;
    for(ckb_key* key = keys; key < maxkey; key++){
        if(key_stamp[keys - key] > 0.0)
            key_stamp[keys - key] -= delta;
        DBG("key %lf", key_stamp[keys - key]);
    }*/
	return;
}

int max(int a, int b){
	return a > b ? a : b;
}

int min(int a, int b){
	return a < b ? a : b;
}

int gcounter = 0;

void getFreqDec(){
	int16_t data[2048] = { 0 };

#ifdef USE_PORTAUDIO
    Pa_ReadStream(stream, data, 512);
    DBG("Read data");
#else
	pa_simple_read(pas, data, sizeof(data), NULL);
#endif

	for(unsigned i = 0; i < 2048; i++){
		inbuf[i].r = data[i] * hann_res[i];
		inbuf[i].i = 0;
	}
	kiss_fft_cfg config = kiss_fft_alloc(2048, 0, NULL, NULL);
	kiss_fft(config, inbuf, outbuf);

	for(unsigned int j=0; j < 1024; j++)
		if(power)
			powers[j] = outbuf[j].r*outbuf[j].r + outbuf[j].i*outbuf[j].i;
		else
			powers[j] = sqrt(outbuf[j].r*outbuf[j].r + outbuf[j].i*outbuf[j].i);
	kiss_fft_free(config);
	kiss_fft_cleanup();

}

#define DIV_FACT 2000000.0
#define NORM_F 0.06
int one = 1;
void translate_frame_vertically(int height){
    for(unsigned int i = 0; i < RANGE; i++)
        translated_frame[i] = round(height * (powers[i]/(DIV_FACT * 3)) * (NORM_F * (i + 1) + 2));

    /*for(unsigned int i=50; i < RANGE-300; i++)
    {
        //powers[i] = (((endmax - endmin)*(i - startmin))/(startmax - startmin))+endmin;
        //translated_frame[i] = round(height * (powers[i]/24000000.0));
        translated_frame[i] = round(height * (powers[i]/DIV_FACT));
    }
    
    for(unsigned int i=RANGE-300; i < RANGE; i++)
        translated_frame[i] = round(height * (powers[i]/(DIV_FACT / 2)));*/
}

/*double gen_key_stamp(int y)
{
    DBG("Eh %d", y/10.0);
    return y / 10.0;
}*/

int get_key_value(int width, int x)
{
    int cur_pos = RANGE * x / width;
    cur_pos = (cur_pos > RANGE - 1 ? RANGE - 1 : cur_pos);
    int newval = translated_frame[cur_pos];
    if(cur_pos > 17)
    {
        for(int i = 0; i < 18; i++)
            if(translated_frame[cur_pos - i] > newval)
                newval = translated_frame[cur_pos - i];
    }
    if(one)
        DBG("%d %d", newval, x);
    return newval;
}
#define V_OFFSET 0
int ckb_frame(ckb_runctx* context){
  /*  if(flip)
    {*/
    CKB_KEYCLEAR(context);
    /*    flip = 0;
    }*/
    ckb_key* keys = context->keys;
	ckb_key* maxkey = keys+context->keycount-1;
	getFreqDec();
    translate_frame_vertically(context->height);
	for(ckb_key* key = keys; key < maxkey; key++){

        int key_y = context->height - key->y;
        unsigned char col = 0 - (key_y + V_OFFSET <= get_key_value(context->width, key->x));
        key->r = key->g = key->b = 0xFF;
        if(col)
        {
            key->a = col;
            //key_stamp[keys - key] = gen_key_stamp(key_y);
            continue;
        }
        //if(key_stamp[keys - key] <= 0.0)
        //{
        //    key->a = 0;
        //}
	}
        one = 0;

    return 0;
}
