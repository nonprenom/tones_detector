#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>

#define SAMPLING_RATE 8000
#define MAX_PATTERN_SIZE 1500
#define NUMSAMPLES 80
#define AVGN 20
#define SIGNAL_FOUND 90 // %
#define SAMPLES_DIR "./pcm8kMono16ble/"
#define NBTESTS 1

enum CODEC_toneType_e
{
	CODEC_TONE_RING,
	CODEC_TONE_BUSY,
};

typedef struct bit
{
	unsigned char b : 1;
} bit_t;

typedef struct tone
{
	char country[64];
	enum CODEC_toneType_e toneType;
	char toneName[64];
	uint16_t frqn;		  // in Hz
	uint16_t durationOn;  // in ms
	uint16_t durationOff; // in ms
	bit_t pattern[MAX_PATTERN_SIZE];
	int pattern_size;
} tone_t;

void build_pattern(tone_t *tone, const int numSamples, const int samplingRate)
{
	int pos = 0;
	int s = 0;

	int N_duration = numSamples * 1000 / samplingRate; // in ms
	fprintf(stderr, "Building pattern [%s] for [%s]...\n", tone->toneName, tone->country);

	// buid the pattern OFF-ON-OFF-ON-OFF
	int model[5] = {0, 1, 0, 1, 0};
	int duration;

	for (int i = 0; i < 5; i++)
	{
		duration = model[i] ? tone->durationOn : tone->durationOff;
		fprintf(stderr, "Freqn %d %s during shapes[%d].duration / N_duration=%d\n",
				tone->frqn, (model[i] ? "ON" : "OFF"), i, duration / N_duration);
		for (int b = 0; b < duration / N_duration; b++)
		{
			tone->pattern[pos].b = model[i];
			pos++;
		}
		s++;
	}
	fprintf(stderr, "pattern_size = %d\r\n", pos);

	tone->pattern_size = pos;
}

int read_tones_def(tone_t tones[])
{
	uint8_t toneId;
	FILE *f = fopen("ring_tones_defs.csv", "r");

	if (!f)
	{
		fprintf(stderr, "error fopen ring_tones_defs.csv\n");
		return 0;
	}
	else
	{
		fprintf(stderr, "processing fopen ring_tones_defs.csv\n");
	}

	int res;
	char line[256];
	while (fscanf(f, "%[^,],%d,%[^,],%d,%d,%d\r\n", tones[toneId].country, &tones[toneId].toneType,
				  tones[toneId].toneName, &tones[toneId].frqn, &tones[toneId].durationOn, &tones[toneId].durationOff) != -1)
	{
		fprintf(stderr, "[%d] %s,%d,%s,%d,%d,%d\r\n",
				toneId, tones[toneId].country, tones[toneId].toneType, tones[toneId].toneName,
				tones[toneId].frqn, tones[toneId].durationOn, tones[toneId].durationOff);
		build_pattern(&tones[toneId], NUMSAMPLES, SAMPLING_RATE);
		toneId++;
	}

	fclose(f);

	return toneId;
}

int pattern_mag(const bit_t data[], const bit_t pattern[], const uint8_t size_of_pattern)
{
	uint16_t i, count = 0;
	for (i = 0; i < size_of_pattern; i++)
	{
		if ((data[i].b == pattern[i].b))
		{
			count++;
		}
	}
	return count * 100 / size_of_pattern;
}

float goertzel_mag(const uint8_t numSamples, const uint16_t targetFrqn, const uint16_t samplingRate, const uint16_t *data)
{
	float coeff, cosine, sine, scalingFactor;

	float real, imag;
	int q0 = 0;
	int q1 = 0;
	int q2 = 0;

	int k;
	float floatnumSamples, omega;

	scalingFactor = numSamples / 2.0;

	floatnumSamples = (float)numSamples;
	k = (int)(0.5 + ((floatnumSamples * targetFrqn) / samplingRate));
	omega = (2.0 * M_PI * k) / floatnumSamples;
	sine = sin(omega);
	cosine = cos(omega);
	coeff = 2.0 * (cosine);

	for (uint8_t i = 0; i < numSamples; i++)
	{
		q0 = coeff * q1 - q2 + (float)data[i];
		q2 = q1;
		q1 = q0;
	}

	// calculate the real and imaginary results
	// scaling appropriately
	real = (q1 * cosine - q2) / scalingFactor;
	imag = (q1 * sine) / scalingFactor;

	return sqrtf(real * real + imag * imag);
}

int main()
{
	// 8000 1s
	//   1  0.000125 s (0.125ms)
	//   8   0.001 (1 ms)
	//  80   0.010 (10 ms)

	tone_t tones[25];
	DIR *folder;
	struct dirent *entry;

	int nbTones = read_tones_def(tones);

	chdir(SAMPLES_DIR);
	int output;

	clock_t begin = clock();

	for (int test = 0; test < NBTESTS; test++)
	{
		folder = opendir(".");
		if (folder == NULL)
		{
			perror("Unable to read directory");
			return (1);
		}

		while ((entry = readdir(folder)))
		{
			if (entry->d_type == DT_REG)
			{
				FILE *f;

				f = fopen(entry->d_name, "rb");

				if (!f)
				{
					fprintf(stderr, "error fopen %s\n", entry->d_name);
					return 0;
				}
				else if (test == 0)
				{
					fprintf(stderr, "%s\n", entry->d_name);
				}

				float t = 0;
				int nb_bits_in_data[nbTones];
				int res[nbTones];
				float lastGoertzelRes[AVGN];
				float lastGoertzelRes_avg = 0;
				float lastGoertzelRes_sum = 0;
				float lastGoertzelRes_max = 0;
				int i, j, p, a;
				uint16_t frame[NUMSAMPLES];
				bit_t data[nbTones][MAX_PATTERN_SIZE];

				bzero(data, sizeof(data));
				bzero(nb_bits_in_data, sizeof(nb_bits_in_data));
				bzero(res, sizeof(res));
				bzero(frame, sizeof(frame));
				bzero(lastGoertzelRes, sizeof(lastGoertzelRes));

				while (fread(frame, sizeof(uint16_t) * NUMSAMPLES, 1, f) == 1)
				{
					t += NUMSAMPLES;
					// check how close to the target frqn the frames are (in goertzel magnitude)
					output = (test == 0);
					for (p = 0; p < nbTones; p++)
					{
						if (strlen(tones[p].country) <= strlen(entry->d_name) && !memcmp(tones[p].country, entry->d_name, strlen(tones[p].country)))
						{
							// aliases
							int *_nb_bits_in_data = &nb_bits_in_data[p];
							int _pattern_size = tones[p].pattern_size;
							int *_res = &res[p];
							bit_t *_data = data[p];
							const bit_t *_pattern = tones[p].pattern;

							lastGoertzelRes[AVGN - 1] = goertzel_mag(NUMSAMPLES, tones[p].frqn, SAMPLING_RATE, frame);
							lastGoertzelRes_sum = 0;
							for (a = 1; a < AVGN; a++)
							{
								lastGoertzelRes[a - 1] = lastGoertzelRes[a];
								lastGoertzelRes_sum += lastGoertzelRes[a - 1];
							}
							if (t > (NUMSAMPLES * AVGN))
							{
								lastGoertzelRes_avg = lastGoertzelRes_sum / AVGN;
								if (lastGoertzelRes_avg > lastGoertzelRes_max)
									lastGoertzelRes_max = lastGoertzelRes_avg;
							}
							else
								lastGoertzelRes_avg = 0;
							_data[*_nb_bits_in_data].b = lastGoertzelRes_avg > (lastGoertzelRes_max / 2);

							if (output)
								printf("%.3f, %.3f, %d\r\n", (t * 1000 / SAMPLING_RATE) / 1000, lastGoertzelRes[AVGN - 1], 30000 + 10000 * (lastGoertzelRes_avg > (lastGoertzelRes_max / 2)));
							output = 0;

							(*_nb_bits_in_data)++;
							(*_res) = 0;
							// wait for enough bits
							if (*_nb_bits_in_data == _pattern_size)
							{
								// get how close to the pattern the data are (in %)
								*_res = (pattern_mag(_data, _pattern, _pattern_size) > SIGNAL_FOUND);
								if (*_res)
								{
									float end = t * 1000 / SAMPLING_RATE;
									float duration = (_pattern_size * NUMSAMPLES) * 1000 / SAMPLING_RATE;
									float start = end - duration;

									if (test == 0)
										fprintf(stderr, "\tp=%d res=%d [%s] [%s] pat_size=%d dur=%.3f start=%.3f end=%.3f\r\n",
												p, *_res, tones[p].country, tones[p].toneName, _pattern_size, duration, start / 1000, end / 1000);
									// if a pattern is found, all the other searchs should now wait again for a full pattern
									for (int i = 0; i < nbTones; i++)
									{
										nb_bits_in_data[i] = 0;
									}
								}
								else
								{
									// move the data one bit to the left for the next compare
									memmove(_data, _data + sizeof(bit_t), _pattern_size - 1);
									(*_nb_bits_in_data)--;
								}
							}
						}
					}
				}
				fclose(f);
			}
		}
		closedir(folder);
	}
	clock_t end = clock();
	double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
	fprintf(stderr, "time = %f\n", time_spent);
	return 0;
}
