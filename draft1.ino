#include <LittleFS.h>
#include <FS.h>
#include <TFT_eSPI.h>
#include <AnimatedGIF.h>

TFT_eSPI tft = TFT_eSPI();
AnimatedGIF gif;

// =====================================================
// FILE CALLBACKS
// =====================================================

void *GIFOpenFile(const char *fname, int32_t *pSize)
{
    File *f = new File(LittleFS.open(fname, "r"));

    if (!(*f))
    {
        delete f;
        return NULL;
    }

    *pSize = f->size();
    return (void *)f;
}

void GIFCloseFile(void *pHandle)
{
    File *f = (File *)pHandle;

    if (f)
    {
        f->close();
        delete f;
    }
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    File *f = (File *)pFile->fHandle;

    if ((pFile->iPos + iLen) > pFile->iSize)
        iLen = pFile->iSize - pFile->iPos;

    int32_t bytesRead = f->read(pBuf, iLen);

    pFile->iPos += bytesRead;

    return bytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
    File *f = (File *)pFile->fHandle;

    f->seek(iPosition);

    pFile->iPos = iPosition;

    return iPosition;
}

// =====================================================
// DRAW CALLBACK (FAST VERSION)
// =====================================================

void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s = pDraw->pPixels;
    uint16_t *palette = pDraw->pPalette;

    static uint16_t lineBuffer[240];

    int xOffset = (240 - gif.getCanvasWidth()) / 2;
    int yOffset = (240 - gif.getCanvasHeight()) / 2;

    int y = yOffset + pDraw->iY + pDraw->y;
    int xBase = xOffset + pDraw->iX;

    if (pDraw->ucHasTransparency)
    {
        uint8_t trans = pDraw->ucTransparent;

        int start = -1;
        int count = 0;

        for (int x = 0; x < pDraw->iWidth; x++)
        {
            if (s[x] != trans)
            {
                if (start == -1)
                {
                    start = x;
                    count = 0;
                }

                lineBuffer[count++] = palette[s[x]];
            }
            else
            {
                if (count)
                {
                    tft.pushImage(
                        xBase + start,
                        y,
                        count,
                        1,
                        lineBuffer
                    );

                    start = -1;
                    count = 0;
                }
            }
        }

        if (count)
        {
            tft.pushImage(
                xBase + start,
                y,
                count,
                1,
                lineBuffer
            );
        }
    }
    else
    {
        for (int x = 0; x < pDraw->iWidth; x++)
        {
            lineBuffer[x] = palette[s[x]];
        }

        tft.pushImage(
            xBase,
            y,
            pDraw->iWidth,
            1,
            lineBuffer
        );
    }
}

// =====================================================
// PLAY GIF
// =====================================================

void playGIF(const char *filename)
{
    if (!gif.open(
            filename,
            GIFOpenFile,
            GIFCloseFile,
            GIFReadFile,
            GIFSeekFile,
            GIFDraw))
    {
        Serial.println("GIF OPEN FAILED");
        return;
    }

    Serial.printf(
        "GIF %s (%d x %d)\n",
        filename,
        gif.getCanvasWidth(),
        gif.getCanvasHeight());

    uint32_t start = millis();
    int frameCount = 0;

    while (gif.playFrame(true, NULL))
    {
        frameCount++;
        yield();
    }

    uint32_t elapsed = millis() - start;

    Serial.printf(
        "Frames: %d  Time: %lu ms\n",
        frameCount,
        elapsed);

    gif.close();
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("ESP8266 FAST GIF PLAYER");

    if (!LittleFS.begin())
    {
        Serial.println("LittleFS FAILED");

        while (1)
        {
            delay(1000);
        }
    }

    Serial.println("LittleFS OK");

    tft.begin();
    tft.setRotation(1);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);

    gif.begin(LITTLE_ENDIAN_PIXELS);

    Serial.println("READY");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{

    playGIF("/1.gif");
    delay(0);
}