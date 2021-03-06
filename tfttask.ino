const uint16_t TFT_BACK_COLOR         = ILI9341_BLACK;
const bool     TFT_SHOW_RAW           = false;            /* show raw PWM values */
const uint16_t TFT_TEXT_COLOR         = ILI9341_YELLOW;
const uint16_t TFT_DATE_COLOR         = ILI9341_WHITE;
const uint16_t TFT_TEMP_COLOR         = ILI9341_WHITE;
const uint8_t  TFT_BACKLIGHT_BITDEPTH = 16;               /*min 11 bits, max 16 bits */
const uint8_t  TFT_BACKLIGHT_CHANNEL  = NUMBER_OF_CHANNELS;
const uint16_t TFT_BUTTON_WIDTH       = 100;
const uint16_t TFT_BUTTON_HEIGHT      =  40;

enum displayState {
  normal, menu
};

struct tftPoint_t
{
  uint16_t x;
  uint16_t y;
};

struct button_t
{
  uint16_t x;
  uint16_t y;
  int16_t  w;
  int16_t  h;
  char     text[15];
};

/* buttons used in menu*/
const button_t LIGHTSON_BUTTON
{
  10, 60, TFT_BUTTON_WIDTH, TFT_BUTTON_HEIGHT, "ON"
};

const button_t LIGHTSOFF_BUTTON
{
  10, 115, TFT_BUTTON_WIDTH, TFT_BUTTON_HEIGHT, "OFF"
};

const button_t LIGHTSAUTO_BUTTON
{
  10, 170, TFT_BUTTON_WIDTH, TFT_BUTTON_HEIGHT, "AUTO"
};

const button_t EXIT_BUTTON
{
  210, 170, TFT_BUTTON_WIDTH, TFT_BUTTON_HEIGHT, "EXIT"
};

/* slider for backlight control - note: XPOS is center of slider*/
const uint16_t BL_SLIDER_XPOS = 160;
const uint16_t BL_SLIDER_YPOS = 40;
const uint16_t BL_SLIDER_WIDTH = 60;
const uint16_t BL_SLIDER_HEIGHT = 160;

/* area to check for touch to control backlight */
const button_t BL_SLIDER_AREA
{
  BL_SLIDER_XPOS - BL_SLIDER_WIDTH / 2, BL_SLIDER_YPOS, BL_SLIDER_WIDTH, BL_SLIDER_HEIGHT
};

/* buttons used on main screen */
const button_t MENU_BUTTON
{
  210, 10, TFT_BUTTON_WIDTH, TFT_BUTTON_HEIGHT, "MENU"
};

button_t tempArea[MAX_NUMBER_OF_SENSORS];

uint16_t backlightMaxvalue;

displayState tftState = normal;

bool tftClearScreen = true;

void tftTask( void * pvParameters )
{
  const uint64_t tftTaskdelayTime = ( 1000 / UPDATE_FREQ_TFT) / portTICK_PERIOD_MS;

  tft.fillScreen( TFT_BACK_COLOR );

  /* setup backlight pwm */
  ledcAttachPin( TFT_BACKLIGHT_PIN, TFT_BACKLIGHT_CHANNEL );
  double backlightFrequency = ledcSetup( TFT_BACKLIGHT_CHANNEL , LEDC_MAXIMUM_FREQ, TFT_BACKLIGHT_BITDEPTH );

  backlightMaxvalue = ( 0x00000001 << TFT_BACKLIGHT_BITDEPTH ) - 1;

  tftBrightness = preferences.getFloat( "tftbrightness", tftBrightness );
  ledcWrite( TFT_BACKLIGHT_CHANNEL, map( tftBrightness, 0, 100, 0, backlightMaxvalue ) );

  ( preferences.getString( "tftorientation", "normal" ) == "normal" ) ? tftOrientation = TFT_ORIENTATION_NORMAL : tftOrientation = TFT_ORIENTATION_UPSIDEDOWN;
  tft.setRotation( tftOrientation );
  while ( !xDimmerTaskHandle )
  {
    vTaskDelay( 10 / portTICK_PERIOD_MS );
  }
  ledcWrite( TFT_BACKLIGHT_CHANNEL, 0 );

  while (1)
  {
    switch ( tftState )
    {
      case normal:
        showStatus();
        vTaskDelay( tftTaskdelayTime );
        break;

      case menu:
        showMenu();
        vTaskDelay( 10 / portTICK_PERIOD_MS );
        break;

      default:
        break;
    }
  }
}

static inline __attribute__((always_inline)) void showMenu()
{
  static lightStatus_t tftLightStatus;

  if ( tftClearScreen )
  {
    const button_t lightButtonsLabel = { LIGHTSON_BUTTON.x, uint16_t( LIGHTSON_BUTTON.y - 35 ), LIGHTSON_BUTTON.w, LIGHTSON_BUTTON.h, "LIGHTS" };

    tft.fillScreen( ILI9341_BLACK );
    drawButton( lightButtonsLabel, ILI9341_YELLOW, 0, 0, 2 );
    drawMenuButtons();
    drawBacklightSlider( BL_SLIDER_AREA, "backlight" );
    ledcWrite( TFT_BACKLIGHT_CHANNEL, map( tftBrightness, 0, 100, 0, backlightMaxvalue ) );
    tftClearScreen = false;
    tftLightStatus = lightStatus;
  }

  /* check if tftBrightness has changed */
  if ( map( tftBrightness, 0, 100, 0, backlightMaxvalue ) != ledcRead( TFT_BACKLIGHT_CHANNEL ) )
  { /* set new backlight value */
    ledcWrite( TFT_BACKLIGHT_CHANNEL, map( tftBrightness, 0, 100, 0, backlightMaxvalue ) );
    drawBacklightSlider( BL_SLIDER_AREA, "" );
  }

  /* check if light status has changed */
  if ( tftLightStatus != lightStatus )
  {
    drawMenuButtons();
    tftLightStatus = lightStatus;
  }

  if ( touch.tirqTouched() )
  {
    TS_Point p = touch.getPoint();

    tftPoint_t clickedLocation;
    clickedLocation = mapToTft( p.x, p.y );

    if ( buttonPressed( LIGHTSON_BUTTON , clickedLocation ) )
    {
      lightsOn();
    }
    else if ( buttonPressed( LIGHTSOFF_BUTTON , clickedLocation ) )
    {
      lightsOff();
    }
    else if ( buttonPressed( LIGHTSAUTO_BUTTON , clickedLocation ) )
    {
      lightsAuto();
    }
    else if ( buttonPressed( BL_SLIDER_AREA , clickedLocation ) )
    {
      static uint16_t oldLocation;
      if ( !( clickedLocation.y == oldLocation ) )
      {
        tftBrightness = map( clickedLocation.y , BL_SLIDER_YPOS, BL_SLIDER_HEIGHT + BL_SLIDER_YPOS, 100, 0 );
        oldLocation = clickedLocation.y;
      }
    }
    else if ( buttonPressed( EXIT_BUTTON , clickedLocation ) )
    {
      tftClearScreen = true;
      tftState = normal;
    }
  }
}

static inline __attribute__((always_inline)) void showStatus()
{
  const uint16_t BARS_BOTTOM      = 190;
  const uint16_t BARS_HEIGHT      = BARS_BOTTOM;
  const uint16_t BARS_BORDER      = 12;
  const uint16_t BARS_WIDTH       = 210 / 5; /* note: total width is 210 px */
  const float    HEIGHT_FACTOR    = BARS_HEIGHT / 100.0;

  static wl_status_t   currentWiFiStatus;
  static lightStatus_t tftLightStatus;

  button_t networkArea
  {
    140, 205, 170, 30
  };

  button_t clockArea
  {
    10, 205, 110, 30
  };

  uint16_t channelColor565[NUMBER_OF_CHANNELS];

  if ( tftClearScreen )
  {
    tft.fillScreen( ILI9341_BLACK );
    drawButton( MENU_BUTTON, ILI9341_YELLOW, ILI9341_BLUE, 0, 1 );

    showIPAddress( networkArea );
    currentWiFiStatus = WiFi.status();

    //tft.startWrite();
    for ( uint8_t thisSensor = 0; thisSensor < numberOfFoundSensors; thisSensor++ )
    {
      tempArea[thisSensor].x = 220;
      tempArea[thisSensor].y = 70 + thisSensor * 50;
      tempArea[thisSensor].w = TFT_BUTTON_WIDTH - 20;
      tempArea[thisSensor].h = 30;
      //tft.writeFastHLine( tempArea[thisSensor].x, tempArea[thisSensor].y, tempArea[thisSensor].w, ILI9341_GREEN );
      //tft.writeFastHLine( tempArea[thisSensor].x, tempArea[thisSensor].y + tempArea[thisSensor].h, tempArea[thisSensor].w, ILI9341_GREEN );
      //tft.writeFastVLine( tempArea[thisSensor].x, tempArea[thisSensor].y, tempArea[thisSensor].h, ILI9341_GREEN );
      //tft.writeFastVLine( tempArea[thisSensor].x + tempArea[thisSensor].w, tempArea[thisSensor].y, tempArea[thisSensor].h, ILI9341_GREEN );
    }
    //tft.endWrite();

    /* draw temp sensor names */
    tft.setTextSize( 0 );
    for ( uint8_t thisSensor = 0; thisSensor < numberOfFoundSensors; thisSensor++ )
    {
      int16_t x, y;
      uint16_t w, h;
      tft.getTextBounds( sensor[thisSensor].name, 0, 0, &x, &y, &w, &h);
      tft.setCursor( ( tempArea[thisSensor].x + tempArea[thisSensor].w / 2 ) - w / 2,
                     ( tempArea[thisSensor].y - 6 ) );
      tft.print( sensor[thisSensor].name );
    }
    drawSensors( true );
  }

  static uint16_t oldPercentage[NUMBER_OF_CHANNELS];
  static uint16_t oldColor565[NUMBER_OF_CHANNELS];

  tft.startWrite();
  for ( uint8_t channelNumber = 0; channelNumber < NUMBER_OF_CHANNELS; channelNumber++ )
  {
    uint32_t color = strtol( &channel[channelNumber].color[1], NULL, 16 );
    channelColor565[channelNumber] = tft.color565( ( color & 0xFF0000 ) >> 16, ( color & 0x00FF00 ) >> 8, color & 0x0000FF  );

    /* only draw channels that changed percentage or color */
    if ( tftClearScreen || oldPercentage[ channelNumber ] != channel[channelNumber].currentPercentage ||
         oldColor565[ channelNumber ] != channelColor565[ channelNumber ] )
    {
      // redraw the top part of the bar
      tft.writeFillRect( channelNumber * BARS_WIDTH + BARS_BORDER,
                         BARS_BOTTOM - BARS_HEIGHT,
                         BARS_WIDTH - BARS_BORDER * 2,
                         BARS_HEIGHT - channel[channelNumber].currentPercentage * HEIGHT_FACTOR,
                         TFT_BACK_COLOR );
      /*
            //100% water mark
            tft.drawFastHLine( channelNumber * BARS_WIDTH + BARS_BORDER,
                               BARS_BOTTOM - BARS_HEIGHT - 1,
                               BARS_WIDTH - BARS_BORDER * 2,
                               tft.color565( r, g, b ) );
      */

      // redraw the bottom part of the bar
      tft.writeFillRect( channelNumber * BARS_WIDTH + BARS_BORDER,
                         BARS_BOTTOM - channel[channelNumber].currentPercentage * HEIGHT_FACTOR,
                         BARS_WIDTH - BARS_BORDER * 2,
                         channel[channelNumber].currentPercentage * HEIGHT_FACTOR,
                         channelColor565[channelNumber]);
    }
  }
  tft.endWrite();

  uint32_t averageLedBrightness = 0;

  for ( uint8_t channelNumber = 0; channelNumber < NUMBER_OF_CHANNELS; channelNumber++ )
  {
    /* only write percentage if changed */
    if ( tftClearScreen || channel[channelNumber].currentPercentage != oldPercentage[channelNumber] )
    {
      button_t label;

      if ( TFT_SHOW_RAW )
      {
        snprintf( label.text, sizeof( label.text ), "%04X", ledcRead( channelNumber ) );
      }
      else
      {
        snprintf( label.text, sizeof( label.text ), " %.1f%% ", channel[channelNumber].currentPercentage );
      }

      label.x = channelNumber * BARS_WIDTH;
      label.y = BARS_BOTTOM + 4;
      label.w = BARS_WIDTH;
      label.h = 10;
      //tft.setTextColor( channelColor565[channelNumber] , TFT_BACK_COLOR );
      drawButton( label, channelColor565[channelNumber], 0, 0, 0 );
    }
    oldColor565[ channelNumber ] = channelColor565[ channelNumber];
    oldPercentage[ channelNumber ] = channel[channelNumber].currentPercentage;
    averageLedBrightness += ledcRead( channelNumber );
  }
  tftClearScreen = false;

  averageLedBrightness = averageLedBrightness / NUMBER_OF_CHANNELS;

  uint16_t rawBrightness = map( tftBrightness, 0, 100, 0, backlightMaxvalue );

  ledcWrite( TFT_BACKLIGHT_CHANNEL, ( averageLedBrightness > rawBrightness ) ? rawBrightness : averageLedBrightness );

  drawSensors( false );

  struct tm timeinfo;

  getLocalTime( &timeinfo );

  static uint16_t oldtimeinfo;

  if ( timeinfo.tm_sec != oldtimeinfo );
  {
    strftime( clockArea.text, sizeof( clockArea.text ), "%T", &timeinfo );
    drawButton( clockArea, ILI9341_YELLOW, 0, 0, 2 );
    oldtimeinfo = timeinfo.tm_sec;
  }

  if ( currentWiFiStatus != WiFi.status() )
  {
    showIPAddress( networkArea );
    currentWiFiStatus = WiFi.status();
  }

  if ( touch.tirqTouched() )
  {
    tftPoint_t clickedLocation;

    TS_Point p = touch.getPoint();
    clickedLocation = mapToTft( p.x, p.y );
    if ( buttonPressed( MENU_BUTTON , clickedLocation ) )
    {
      tftState = menu;
      tftClearScreen = true;
    }
  }
}

static inline __attribute__((always_inline)) void drawButton( struct button_t button, const uint16_t labelcolor, const uint16_t color, const uint16_t bordercolor, const uint8_t fontsize )
{
  if ( color || bordercolor )
  {
    tft.startWrite();
    if ( color )
    {
      tft.writeFillRect( button.x, button.y, button.w, button.h, color );
    }
    if ( bordercolor )
    {
      tft.writeFastHLine( button.x, button.y, button.w, bordercolor );
      tft.writeFastHLine( button.x, button.y + button.h, button.w, bordercolor );
      tft.writeFastVLine( button.x, button.y, button.h, bordercolor );
      tft.writeFastVLine( button.x + button.w, button.y, button.h, bordercolor );
    }
    tft.endWrite();
  }

  int16_t x, y;
  uint16_t w, h;
  tft.setTextSize( fontsize );
  tft.getTextBounds( button.text, 0, 0, &x, &y, &w, &h);
  tft.setCursor( ( button.x + button.w / 2 ) - w / 2,
                 ( button.y + button.h / 2 ) - h / 2 );
  tft.setTextColor( labelcolor, color );
  tft.print( button.text );
}

static inline __attribute__((always_inline)) void drawMenuButtons()
{
  drawButton( LIGHTSON_BUTTON,     ILI9341_YELLOW, lightStatus == LIGHTS_ON  ? ILI9341_RED : ILI9341_BLUE, 0, 2 );
  drawButton( LIGHTSOFF_BUTTON,    ILI9341_YELLOW, lightStatus == LIGHTS_OFF ? ILI9341_RED : ILI9341_BLUE, 0, 2 );
  drawButton( LIGHTSAUTO_BUTTON,   ILI9341_YELLOW, lightStatus == LIGHTS_AUTO ? ILI9341_RED : ILI9341_BLUE, 0, 2 );
  drawButton( EXIT_BUTTON,         ILI9341_YELLOW, ILI9341_BLUE, 0, 2 );
}

static inline __attribute__((always_inline)) bool buttonPressed( const struct button_t button, const struct tftPoint_t clickedLocation )
{
  return ( clickedLocation.x > button.x && clickedLocation.x < button.x + button.w ) && ( clickedLocation.y > button.y && clickedLocation.y < button.y + button.h );
}

static inline __attribute__((always_inline)) struct tftPoint_t mapToTft( const uint16_t touchX, const uint16_t touchY )
{
  uint16_t x, y;
  if ( !TOUCH_IS_INVERTED )
  {
    if (  tftOrientation == TFT_ORIENTATION_UPSIDEDOWN )
    {
      x = mapFloat( touchX, 340, 3900, 0, 320 );
      y = mapFloat( touchY, 200, 3850, 0, 240 );
    }
    else if ( tftOrientation == TFT_ORIENTATION_NORMAL )
    {
      x = mapFloat( touchX, 340, 3900, 320, 0 );
      y = mapFloat( touchY, 200, 3850, 240, 0 );
    }
  }
  else
  {
    if (  tftOrientation == TFT_ORIENTATION_UPSIDEDOWN )
    {
      x = mapFloat( touchX, 340, 3900, 320, 0 );
      y = mapFloat( touchY, 200, 3850, 240, 0 );
    }
    else if ( tftOrientation == TFT_ORIENTATION_NORMAL )
    {
      x = mapFloat( touchX, 340, 3900, 0, 320 );
      y = mapFloat( touchY, 200, 3850, 0, 240 );
    }
  }
  return { x, y };
}

static inline __attribute__((always_inline)) void drawBacklightSlider( const button_t area, const char * label )
{
  const uint8_t SLIDER_KNOB_HEIGHT = 10;

  static button_t sliderKnob = { area.x, area.y, area.w, SLIDER_KNOB_HEIGHT };

  tft.startWrite();
  tft.writeFillRect( sliderKnob.x, sliderKnob.y, sliderKnob.w, sliderKnob.h, ILI9341_BLACK);
  tft.writeFillRect( area.x + area.w / 2  - 1, area.y, 2, area.h, ILI9341_YELLOW );
  tft.endWrite();

  //drawButton( area, 0, 0, ILI9341_GREEN, 0 ); //debug

  if ( label )
  {
    //print label under the slider
    sliderKnob.y = area.y + area.h + 8;
    snprintf( sliderKnob.text, sizeof( sliderKnob.text ), "%s", label );
    drawButton( sliderKnob, ILI9341_YELLOW, 0, 0, 0 );
  }

  //print percentage above the slider
  sliderKnob.y = area.y - 17;
  snprintf( sliderKnob.text, sizeof( sliderKnob.text ), " %.0f%% ", tftBrightness ); /* extra spaces to overwrite old value */
  drawButton( sliderKnob, ILI9341_YELLOW, 0, 0, 2 );

  //draw the slider knob
  uint16_t ypos = map( tftBrightness, 0, 100, area.h + area.y, area.y );
  sliderKnob.y = ypos;
  sliderKnob.text[0] = 0;
  drawButton( sliderKnob, 0, ILI9341_BLUE, 0, 0 );
}

static inline __attribute__((always_inline)) void showIPAddress( button_t area )
{
  tft.startWrite();
  tft.writeFillRect( area.x, area.y, area.w, area.h, ILI9341_BLACK );
  tft.endWrite();

  tcpip_adapter_ip_info_t ip_info;

  ESP_ERROR_CHECK( tcpip_adapter_get_ip_info( TCPIP_ADAPTER_IF_STA, &ip_info ) );
  snprintf( area.text, sizeof( area.text ), "%s", ip4addr_ntoa( &ip_info.ip ) );
  drawButton( area, ILI9341_YELLOW, 0, 0, 2 );
}

static inline __attribute__((always_inline)) void drawSensors( const bool forceDraw )
{
  if ( numberOfFoundSensors )
  {
    static float currentTemp[MAX_NUMBER_OF_SENSORS];

    for ( uint8_t thisSensor = 0; thisSensor < numberOfFoundSensors; thisSensor++ )
    {
      if ( sensor[ thisSensor ].tempCelcius != currentTemp[ thisSensor ] || forceDraw )
      {
        snprintf( tempArea[thisSensor].text, sizeof( tempArea[thisSensor].text ), "%.1f%c", sensor[thisSensor].tempCelcius, char(247) );
        drawButton( tempArea[thisSensor], TFT_TEMP_COLOR, 0, 0, 2 );
        currentTemp[ thisSensor ] = sensor[ thisSensor ].tempCelcius;
      }
    }
  }
}
