#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <LittleFS.h>
#include <FS.h>

#include <TFT_eSPI.h>
#include <AnimatedGIF.h>

// =====================================================
// ACCESS POINT
// =====================================================

const char* AP_SSID = "ESP-GIF-DISPLAY";
const char* AP_PASS = "12345678";

// =====================================================
// OBJECTS
// =====================================================

ESP8266WebServer server(80);

TFT_eSPI tft = TFT_eSPI();
AnimatedGIF gif;

// =====================================================
// GIF STATE
// =====================================================

String currentGif = "/1.gif";

bool gifOpened = false;
volatile bool gifChanged = false;

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
// DRAW CALLBACK
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
            lineBuffer);

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
        lineBuffer);
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
      lineBuffer);
  }
}

// =====================================================
// WEB PAGE
// =====================================================

String buildButtons()
{
  String html;

  Dir dir = LittleFS.openDir("/");

  while (dir.next())
  {
    String file = dir.fileName();

    if (file.endsWith(".gif"))
    {
      html += "<button onclick=\"setGif('";
      html += file;
      html += "')\">";
      html += file;
      html += "</button><br><br>";
    }
  }

  return html;
}

void handleRoot()
{
  String html;

  html += "<!DOCTYPE html>";
  html += "<html><head>";

  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";

  html += "<style>";
  html += "body{background:#111;color:white;font-family:Arial;text-align:center;padding:20px;}";
  html += "button{width:220px;height:60px;font-size:18px;border:none;border-radius:12px;}";
  html += "</style>";

  html += "<script>";
  html += "function setGif(name){";
  html += "fetch('/select?gif='+name);";
  html += "document.getElementById('current').innerHTML=name;";
  html += "}";
  html += "</script>";

  html += "</head><body>";

  html += "<h2>ESP8266 GIF Controller by ralfzz ft GPT</h2>";

  html += "<p>Current GIF</p>";

  html += "<h3 id='current'>";
  html += currentGif;
  html += "</h3>";

  html += buildButtons();

  html += "</body></html>";

  server.send(200, "text/html", html);
}

// =====================================================
// SELECT GIF
// =====================================================

void handleSelect()
{
  if (server.hasArg("gif"))
  {
    String file = server.arg("gif");

    if (!file.startsWith("/"))
      file = "/" + file;

    currentGif = file;

    gifChanged = true;

    Serial.print("Selected: ");
    Serial.println(currentGif);
  }

  server.send(200, "text/plain", "OK");
}

// =====================================================
// OPEN GIF
// =====================================================

bool openCurrentGif()
{
  if (gif.open(
        currentGif.c_str(),
        GIFOpenFile,
        GIFCloseFile,
        GIFReadFile,
        GIFSeekFile,
        GIFDraw))
  {
    Serial.print("Playing ");
    Serial.println(currentGif);

    return true;
  }

  Serial.println("GIF OPEN FAILED");

  return false;
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("ESP8266 GIF WEB PLAYER");

  // ---------- LittleFS ----------

  if (!LittleFS.begin())
  {
    Serial.println("LittleFS FAILED");

    while (1)
      delay(1000);
  }

  Serial.println("LittleFS OK");

  Dir dir = LittleFS.openDir("/");

  while (dir.next())
  {
    Serial.print("File: ");
    Serial.println(dir.fileName());
  }

  // ---------- TFT ----------

    tft.begin();
    tft.setRotation(1);
    tft.setSwapBytes(true);

    tft.fillScreen(TFT_BLACK);
    delay(50);
    tft.fillScreen(TFT_BLACK);
  

  gif.begin(LITTLE_ENDIAN_PIXELS);

  // ---------- AP MODE ----------

  WiFi.mode(WIFI_AP);

  WiFi.softAP(
    AP_SSID,
    AP_PASS
  );

  Serial.println();
  Serial.println("Access Point Started");

  Serial.print("SSID : ");
  Serial.println(AP_SSID);

  Serial.print("IP   : ");
  Serial.println(WiFi.softAPIP());

  // ---------- WEB SERVER ----------

  server.on("/", handleRoot);
  server.on("/select", handleSelect);

  server.begin();

  Serial.println("Web Server Started");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  server.handleClient();

  if (!gifOpened)
  {
    gifOpened = openCurrentGif();
  }

 if (gifChanged)
{
    gif.close();

    tft.fillScreen(TFT_BLACK);

    delay(10);

    gifOpened = openCurrentGif();

    gifChanged = false;
}

  if (gifOpened)
  {
    if (!gif.playFrame(true, NULL))
    {
      gif.close();

      gifOpened = openCurrentGif();
    }
  }

  yield();
}