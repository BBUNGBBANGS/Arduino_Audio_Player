//#include"timer-api.h"
#include "wave_header.h"
#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal.h>

#define PHASE_1  3
#define PHASE_2  1
#define PHASE_3  0
#define PHASE_4  2

#define MUSIC_STOPPED   0
#define MUSIC_PLAYING   1

#define MUSIC_PLAYING_INIT      1
#define MUSIC_PLAYING_BUFFER_1  2 
#define MUSIC_PLAYING_BUFFER_2  3
#define MUSIC_PLAYING_FINISHED  4

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

File root;    
File Music;
uint8_t Encoder_Status = 3;
uint8_t Encoder_Push_Status;
int8_t Encoder_Counter;
String WavFileName[20];
uint16_t WavFileCount;

Wave_Header_t WavFile;
uint8_t Music_Buffer[2][3000];
uint8_t Music_Playing_Status = MUSIC_PLAYING_INIT;
uint32_t Music_Playing_BufCounter;
uint32_t Music_Playing_BufCounter1;
uint32_t Music_Playing_BufCounter2;

void setup() 
{
    pinMode(18,INPUT);
    pinMode(19,INPUT);
    pinMode(20,INPUT);
    pinMode(6,OUTPUT);
    pinMode(7,OUTPUT);
    pinMode(9,OUTPUT);
    pinMode(10,OUTPUT);

    External_ISR_Init();
    lcd.begin(16, 2);

    Serial.begin(115200);

    Serial.print("Initialization SD Card...");
    if (!SD.begin(53))
    {
        Serial.println("Initialization failed!");
    }
    Serial.println("Initialization done.");
    root = SD.open("/");
    printDirectory(root,0);
    Serial.println("done!");

    Timer1_Init();
    Timer2_Init();
    Timer4_Init();

    SREG = 0x80;
    Serial.println("Init Finished");
}

void loop() 
{
	Task_100ms();
    Music_Play();
}

void External_ISR_Init(void)
{
    /* INT1 : Falling-Edge / INT2 : Any-Edge / INT3 : Any-Edge */
    EICRA = 0x58;
    /* INT1 , INT2 , INT3 Enable */
    EIMSK = 0x0E;
}

void Timer1_Init(void)
{
    TCCR1A = 0x00;
    TCCR1B = 0x09; //CTC Mode , Prescale = 1, 1 Tick당 0.0625[us]
    TIMSK1 = 0x02; //Timer1 Output Compare A Match Interrupt Enable
    OCR1A = 0xFF;
}

void Timer2_Init(void)
{
    TCCR2A = 0xA3;
    TCCR2B = 0x01; //FAST PWM Mode , Prescale = 1, 1 Tick당 0.0625[us]
}

void Timer4_Init(void)
{
    TCCR4A = 0xA1;
    TCCR4B = 0x09; //FAST PWM Mode, Prescale = 1, 1 Tick당 0.0625[us]
}
void printDirectory(File dir, int numTabs)
{
    while(true)
    {
        File entry = dir.openNextFile();
        if(!entry)
        {
            break;
        }
        for(uint8_t i = 0; i < numTabs; i++)
        {
            Serial.print('\t');
        }
        Serial.print(entry.name());
        WavFileName[WavFileCount] = entry.name();
        WavFileCount++;
        if(entry.isDirectory())
        {
            Serial.println("/");
            printDirectory(entry, numTabs + 1);
        }
        else
        {
            Serial.print("\t\t");
            Serial.println(entry.size(),DEC);
        }
        entry.close();
    }
    if(WavFileCount>3)
    {
        WavFileCount = WavFileCount - 3;
    }

}

void Task_100ms(void)
{
    static int8_t count_old; 
    if(Encoder_Push_Status == MUSIC_STOPPED)
    {
        if(Encoder_Counter>WavFileCount)
        {
            Encoder_Counter = 0;
        }
        if(Encoder_Counter<0)
        {
            Encoder_Counter = WavFileCount;
        }
        if(count_old != Encoder_Counter)
        {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(WavFileName[3 + Encoder_Counter]);
            lcd.setCursor(0, 1);
            lcd.print("STOPPED");
        }
        count_old = Encoder_Counter;
        //Serial.print("Encoder counter : ");
        //Serial.print(Encoder_Counter);
        //Serial.print(", WavFile Coutner : ");
        //Serial.println(WavFileCount);
        //Serial.println(" , MUSIC PLAY STOPPED");
    }
    else
    {
        //lcd.setCursor(0, 1);
        //lcd.print("PLAYING");  
        //Serial.println("MUSIC PLAYING");   
    }
    
    //delay(100);
}


void Music_Play(void)
{
    uint8_t *Ptr = NULL;
    static uint8_t Status_Old;
    float bps_to_count = 0.0f;
    if(Encoder_Push_Status == MUSIC_PLAYING)
    {
        switch(Music_Playing_Status)
        {
            case MUSIC_PLAYING_INIT : 
            {
                Music = SD.open(WavFileName[3 + Encoder_Counter]);
                Ptr = &WavFile.Riff.ChunkID[0];
                Music_Playing_BufCounter = 0;
                Music.seek(Music_Playing_BufCounter);
                Music.read(Ptr,WAVFILE_HEADER_LENGTH);
                Music_Playing_BufCounter += WAVFILE_HEADER_LENGTH;
                Music.seek(Music_Playing_BufCounter);
                Music.read(&Music_Buffer[0][0],3000);
                bps_to_count = 1.0f / (float)(WavFile.Fmt.SampleRate) * 1000000 / 0.0625f;
                OCR1A = (uint16_t)bps_to_count;
                Status_Old = Music_Playing_Status;
                Music_Playing_Status = MUSIC_PLAYING_BUFFER_1;
                Serial.println("MUSIC PLAY INIT FINISHED");
            }
            break;
            case MUSIC_PLAYING_BUFFER_1 :
            {
                if((Status_Old == MUSIC_PLAYING_INIT) || (Status_Old == MUSIC_PLAYING_BUFFER_2))
                {
                    Music_Playing_BufCounter += 3000;
                    Music.seek(Music_Playing_BufCounter);
                    Music.read(&Music_Buffer[1][0],3000);
                }
                Status_Old = Music_Playing_Status;
            }
            break;
            case MUSIC_PLAYING_BUFFER_2 : 
            {
                if(Status_Old == MUSIC_PLAYING_BUFFER_1)
                {
                    Music_Playing_BufCounter += 3000;
                    Music.seek(Music_Playing_BufCounter);
                    Music.read(&Music_Buffer[0][0],3000);
                }
                Status_Old = Music_Playing_Status;
            }
            break;
            case MUSIC_PLAYING_FINISHED :
            {
                Music.close();
            }
            break;
            default :        
            break;
        }
    }
    else
    {


    }
}

ISR(INT1_vect)
{
    Encoder_Push_Status = (~Encoder_Push_Status) & 0x01;
}

ISR(INT2_vect)
{
    Encoder_Rotation_State();
}

ISR(INT3_vect)
{
    Encoder_Rotation_State();
}

void Encoder_Rotation_State(void)
{
    static int8_t counter;
    uint8_t PHA,PHB,status;
    
    PHA = digitalRead(18);
    PHB = digitalRead(19);

    status = (PHA<<1) | (PHB);
    if(Encoder_Push_Status == MUSIC_STOPPED)
    {
        switch (status)
        {
            case PHASE_1 :
                if(Encoder_Status == PHASE_4)
                {
                    counter++;
                    if(counter == 4)
                    {
                        Encoder_Counter++;
                        counter = 0;
                    }
                }
                if(Encoder_Status == PHASE_2)
                {
                    counter--;
                    if(counter == -4)
                    {
                        Encoder_Counter--;
                        counter = 0;
                    }
                }
                counter = 0;
            break;
            case PHASE_2 :
                if(Encoder_Status == PHASE_1)
                {
                    counter++;
                }            
                if(Encoder_Status == PHASE_3)
                {
                    counter--;
                }
            break;        
            case PHASE_3 :
                if(Encoder_Status == PHASE_2)
                {
                    counter++;                
                }            
                if(Encoder_Status == PHASE_4)
                {
                    counter--;
                }
            break;        
            case PHASE_4 :
                if(Encoder_Status == PHASE_3)
                {
                    counter++;                
                }            
                if(Encoder_Status == PHASE_1)
                {
                    counter--;
                }
            break;    
            default:
            break;
        }
        Encoder_Status = status;
    }
}

ISR(TIMER1_COMPA_vect)
{
    int16_t sound_data;
    uint16_t convert_data;
    if(Encoder_Push_Status == MUSIC_PLAYING)
    {
        if(Music_Playing_Status == MUSIC_PLAYING_BUFFER_1)
        {
            sound_data = Music_Buffer[0][Music_Playing_BufCounter1] | (Music_Buffer[0][Music_Playing_BufCounter1 + 1]<<8);

            convert_data = sound_data + 0x8000;

            OCR2A = (convert_data >> 8) & 0xFF;
            OCR2B = convert_data & 0xFF;
            OCR4A = (convert_data >> 8) & 0xFF;
            OCR4B = convert_data & 0xFF;
            Music_Playing_BufCounter1 += 2;
            
            if(Music_Playing_BufCounter1 >= 3000)
            {
                Music_Playing_BufCounter1 = 0;
                Music_Playing_Status = MUSIC_PLAYING_BUFFER_2;
            }
        }
        else if(Music_Playing_Status == MUSIC_PLAYING_BUFFER_2)
        {
            sound_data = Music_Buffer[1][Music_Playing_BufCounter2] | (Music_Buffer[1][Music_Playing_BufCounter2 + 1]<<8);

            convert_data = sound_data + 0x8000;

            OCR2A = (convert_data >> 8) & 0xFF;
            OCR2B = convert_data & 0xFF;
            OCR4A = (convert_data >> 8) & 0xFF;
            OCR4B = convert_data & 0xFF;
            Music_Playing_BufCounter2 += 2;
            
            if(Music_Playing_BufCounter2 >= 3000)
            {
                Music_Playing_BufCounter2 = 0;
                Music_Playing_Status = MUSIC_PLAYING_BUFFER_1;
            }
        }
    }
    else
    {
        OCR2A = 0;
        OCR2B = 0;
        OCR4A = 0;
        OCR4B = 0;        
    }
}