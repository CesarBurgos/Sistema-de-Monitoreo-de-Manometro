#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <EEPROM.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Base64.h"
#include "esp_camera.h"

//CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
 
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define led_FLASH 4
#define camara_activa 12
#define conexion_internet 13
#define modo 14

const char* myDomain = "script.google.com";
String myScript = "/macros/s/AKfycbyXfd1GmADwrEisxVQy-AQt_pjfefD_q0ujqx_6tef6L55F9-EosSQYGBS00AHRoyBK/exec";    //Replace with your own url
String myLineNotifyToken = "myToken=**********";
String myFoldername = "&myFoldername=ESP32CAM-MANOMETRO";
String mimeType = "&mimetype=image/jpeg";
String myFilename = "&filename=ESP32CAM-MANOMETRO.jpg";
String myImage = "&myFile=";

String getAll="", getBody = "";

int waitingTime = 90000; //Wait 30 seconds to google response.

//-------------------VARIABLES GLOBALES--------------------------
int contconexion = 0;
unsigned long previousMillis = 0;

char ssid[50];      
char pass[50];
char user[50];

const char *ssidConf = "esp32cam";
const char *passConf = "12345678";

const uint32_t TiempoEsperaWifi = 5000;

unsigned long TiempoActual = 0;
unsigned long TiempoAnterior = 0;
const long TiempoCancelacion = 500;

int banconection = 0; // Bandera que indicará si esta conectado
int bandera_envio = 0;

//-----------CODIGO HTML PARA PAGINAS WEB ---------------
String pagina_p1 = "<!DOCTYPE html>"
  "<html>"
    "<head>"
      "<meta charset='UTF-8'>"
      "<meta http-equiv='X-UA-Compatible' content='IE=edge'>"
      "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
      "<title>ESP32-CAM (Manometro)</title>"
    "</head>"
    "<style>"
      "html{"
        "width: 100%;"
        "height: 100vh;"
        "overflow:hidden;"
      "}"

      ".flex_column{"
        "display: flex;"
        "flex-direction: column;"
        "align-items: center;"
      "}"

      "body{"
        "width: 100%;"
        "height: 100%;"
        "padding: 0;"
        "margin: 0;"
        "background-color: rgb(233, 233, 233);"
      "}"

      "footer{"
        "background-color: black;"
        "width: 100%;"
        "height: fit-content;"
      "}"

      "h1, h2, h3, h4{"
        "font-family: 'Segoe UI', Tahoma, Verdana, sans-serif;"
        "margin: 0;"
        "margin-top: 5px;"
        "margin-bottom: 10px;"
        "text-align: center;"
      "}"

      "input, p{"
        "width: 90%;"
        "font-family: 'Segoe UI', Tahoma, Verdana, sans-serif;"
      "}"

      ".contenido{"
        "background-color: rgb(233, 233, 233);"
        "width: 95%;"
        "height: 100%;"
        "overflow: auto;"
      "}"

      ".apartado{"
        "background-color: rgb(245, 245, 245);"
        "box-shadow: 3px 3px 9px black;"
        "margin: 0 5px;"
        "margin-bottom: 15px;"
        "padding: 0 15px;"
        "width: fit-content;"
      "}"

      ".boton{"
        "font-weight: bolder;"
        "font-size: 1em;"
        "cursor: pointer;"
        "border: 1px solid;"
        "padding: 10px;"
        "background-color: rgb(223, 240, 252);"
      "}"

      ".boton:hover{"
        "box-shadow: 3px 3px 3px gray;"
      "}"

      "footer h4{"
        "color: white;"
      "}"

      ".fondo{"
          "display: none;"
          "position: fixed;"
          "top:0;"
          "margin: 0;"
          "z-index: 1;"
          "background-color: black;"
          "width: 100%;"
          "height: 100vh;"
      "}"

      ".fondo h2{"
        "color:white;"
        "margin: auto;"
      "}"
    "</style>"
    "<body class = 'flex_column'>"
      "<div class = 'contenido flex_column'><br><br>"
      "<h3 style = 'text-align: center;'> Digitalización de información de un manómetro analógico a través de visión artificial </h3><br>";

String paginafin = "</div><br>"
      "<div id = 'mensj' class = 'fondo flex_column'>"
        "<h2 style = 'text-align: center;'> Espere un momento... </h2><br>"
      "</div>"
      "<script>"
        "function mensj_a(){"
          "let mnsj = document.getElementById('mensj');"
          "mnsj.style.display = 'flex';"
        "};"
      "</script>"
    "<footer class = 'flex_column'><br><br>"
      "<h4>Website developed by Cesar Brgs. (2023)</h4><br><br>"
    "</footer>"
  "</body>"
"</html>";

// Para almacenar datos del cliente (Configuración)
String contenido;


//--------------------------------------------------------------
WebServer server(80);
//--------------------------------------------------------------


void saveCapturedImage(){
  Serial.println("Connect to " + String(myDomain));
  WiFiClientSecure client;
  client.setInsecure();
  bandera_envio = 0;

  if(client.connect(myDomain, 443)) {
    Serial.println("Connection successful");

    digitalWrite(led_FLASH, HIGH);
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb); // dispose the buffered image
    fb = NULL; // reset to capture errors
    fb = esp_camera_fb_get(); // get fresh image
    delay(1000);

    if(!fb){
      digitalWrite(led_FLASH, LOW);
      bandera_envio = 0;
      Serial.println("Camera capture failed");
      delay(1000);
      ESP.restart();
      return;
    }

    char *input = (char *)fb->buf;
    char output[base64_enc_len(3)];
    String imageFile = "data:image/jpeg;base64,";
    for (int i=0;i<fb->len;i++) {
      base64_encode(output, (input++), 3);
      if (i%3==0) imageFile += urlencode(String(output));
    }

    //String Data = myFilename+mimeType+myImage;
    String Data = myLineNotifyToken+myFoldername+myFilename+myImage;
    
    client.println("POST " + myScript + " HTTP/1.1");
    client.println("Host: " + String(myDomain));
    client.println("Content-Length: " + String(Data.length()+imageFile.length()));
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Connection: keep-alive");
    client.println();
    
    client.print(Data);
    int Index;
    for (Index = 0; Index < imageFile.length(); Index = Index+1000) {
      client.print(imageFile.substring(Index, Index+1000));
    }
    
    esp_camera_fb_return(fb);
    delay(1000);
    digitalWrite(led_FLASH, LOW);
    
    
    Serial.println("Waiting for response.");
    long int StartTime=millis();
    while (!client.available()) {
      Serial.print(".");
      delay(220);
      digitalWrite(camara_activa, HIGH);
      delay(220);
      digitalWrite(camara_activa, LOW);
      
      if ((StartTime + waitingTime) < millis()) {
        Serial.println();
        Serial.println("No response.");
        bandera_envio = 0;
        //If you have no response, maybe need a greater value of waitingTime
        break;
      }
    }

    bandera_envio = 1;
    Serial.println("Imagen Enviada");
  } else {
    bandera_envio = -1;
    Serial.println("Connected to " + String(myDomain) + " failed.");
  }
  client.stop();
  
  if(bandera_envio == 1){
    int error = 0;
    while(error < 5){
      delay(130);
      digitalWrite(camara_activa, HIGH);
      delay(130);
      digitalWrite(camara_activa, LOW);
      error += 1;
    }
    digitalWrite(camara_activa, HIGH);
    sitio_web();
  }else if(bandera_envio == 0){
    int error = 0;
    while(error < 3){
      digitalWrite(conexion_internet, HIGH);
      digitalWrite(camara_activa, HIGH);
      delay(250);
      
      digitalWrite(conexion_internet, LOW);
      digitalWrite(camara_activa, LOW);
      delay(250);
      error += 1;
    }
    digitalWrite(conexion_internet, HIGH);
    digitalWrite(camara_activa, HIGH);
    error_captura();
  }else{
    int error = 0;
    while(error < 3){
      digitalWrite(conexion_internet, HIGH);
      digitalWrite(camara_activa, HIGH);
      delay(250);
      
      digitalWrite(conexion_internet, LOW);
      digitalWrite(camara_activa, LOW);
      delay(250);
      error += 1;
    }

    digitalWrite(conexion_internet, HIGH);
    digitalWrite(camara_activa, HIGH);
    error_procesamiento();
  }

  contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
        "<h3> Conexión establecida: </h3>"
        "<h4 style= 'width: 90%'> * Conectado a SSID: " + String(ssid) + " </h4>" 
        "<h4 style= 'width: 90%'> * Usuario: " + String(user) + " </h4><br>" 

        "<a href='realizar_lectura'>"
          "<button class='boton'> TOMAR LECTURA DEL MANOMETRO </button>"
        "</a><br>"
        
        "<a href='consultar_lecturas'>"
          "<button class='boton'> CONSULTAR LECTURAS DEL MANOMETRO </button>"
        "</a>"
    "</div><br>";
}

//https://github.com/zenmanenergy/ESP8266-Arduino-Examples/
String urlencode(String str){
  String encodedString="";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i =0; i < str.length(); i++){
    c=str.charAt(i);
    if (c == ' '){
      encodedString+= '+';
    } else if (isalnum(c)){
      encodedString+=c;
    } else{
      code1=(c & 0xf)+'0';
      if ((c & 0xf) >9){
          code1=(c & 0xf) - 10 + 'A';
      }
      c=(c>>4)&0xf;
      code0=c+'0';
      if (c > 9){
          code0=c - 10 + 'A';
      }
      code2='\0';
      encodedString+='%';
      encodedString+=code0;
      encodedString+=code1;
      //encodedString+=code2;
    }
    yield();
  }
  return encodedString;
}

//-------------------PAGINA DE CONFIGURACION--------------------
void paginaconf() {
  server.send(200, "text/html", pagina_p1 + contenido + paginafin); 
}

void paginainicio() {
    contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
        "<h3> Conexión establecida: </h3>"
        "<h4 style= 'width: 90%'> * Conectado a SSID: " + String(ssid) + " </h4>" 
        "<h4 style= 'width: 90%'> * Usuario: " + String(user) + " </h4><br>" 

        "<a href='realizar_lectura'>"
          "<button class='boton'> TOMAR LECTURA DEL MANOMETRO </button>"
        "</a><br>"
        
        "<a href='consultar_lecturas'>"
          "<button class='boton'> CONSULTAR LECTURAS DEL MANOMETRO </button>"
        "</a>"
    "</div><br>";
  server.send(200, "text/html", pagina_p1 + contenido + paginafin); 
}

// Paginas de procesamiento
void sitio_web() {
  contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
        "<script>"
          "location.href ='http://dominic101.pythonanywhere.com/';"
        "</script>"
      "</div><br>";
      //http://dominic101.pythonanywhere.com/
  server.send(200, "text/html", pagina_p1 + contenido + paginafin); 
}

void error_captura(){
    contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
        "<H3>"
          "Ha ocurrido un error en la lectura del manometro..."
        "</H3><br>"
        "<button class = 'boton' onclick='window.history.go(-1);'> Volver </button>"
      "</div><br>";
    server.send(200, "text/html", pagina_p1 + contenido + paginafin);
}

void error_procesamiento(){
    contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
        "<H3>"
          "No puede ser procesada la fotografía capturada al manometro..."
        "</H3><br>"
        "<button class = 'boton' onclick='window.history.go(-1);'> Volver </button>"
      "</div><br>";
    server.send(200, "text/html", pagina_p1 + contenido + paginafin); 
}

void consulta_lects() {
  contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
        "<script>"
          "location.href ='http://dominic101.pythonanywhere.com/lecturas_manometro';"
        "</script>"
      "</div><br>";
      //http://dominic101.pythonanywhere.com/
  server.send(200, "text/html", pagina_p1 + contenido + paginafin); 
}


//--------------------MODO_CONFIGURACION------------------------
void modoconf() {
  delay(100);
  digitalWrite(conexion_internet, HIGH);
  delay(100);
  digitalWrite(conexion_internet, LOW);
  delay(100);
  digitalWrite(conexion_internet, HIGH);
  delay(100);
  digitalWrite(conexion_internet, LOW);

  contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
        "<form action='guardar_conf' method='get' class = 'apartado flex_column'>"
          "<h3> Configuración de Punto de Acceso: </h3>"

          "<h3 style= 'width: 90%; text-align: left;'> * Ingresar SSID: </h3>" 
          "<input class='input1' name='ssid' type='text' autocomplete='off'>"

          "<h3 style= 'width: 90%; text-align: left;'> * Ingresar PASSWORD: </h3>"  
          "<input class='input1' name='pass' type = 'password' autocomplete='off'>"

          "<h3 style= 'width: 90%; text-align: left;'> * Ingresar un NOMBRE DE USUARIO: </h3>"  
          "<input class='input1' name='user_e' type = 'text' autocomplete='off' minlength=10><br>"

          "<input class='boton' type='submit' value = 'GUARDAR CONFIGURACIÓN'/><br>"
        "</form>"
      "</div><br>"

      "<a href='escanear'>"
        "<button class='boton'> ESCANEAR REDES DISPONIBLES </button>"
      "</a>";

  WiFi.softAP(ssidConf, passConf);
  IPAddress myIP = WiFi.softAPIP();

  server.on("/", paginaconf);               // Página de configuración
  server.on("/guardar_conf", guardar_conf); // Graba en la eeprom la configuración
  server.on("/escanear", escanear);         // Escanean las redes WIFI disponibles
  

  if (!MDNS.begin("esp32cam")) {
    while(1){
      delay(1000);
    }
  }
  MDNS.addService("http", "tcp", 80);
  server.begin();

  while(true){
      server.handleClient();
  }
}

//--------------------- GUARDAR CONFIGURACION -------------------------
void guardar_conf() {
  //Serial.println(server.arg("ssid"));//Recibimos los valores que envia por GET el formulario web
  grabar(0,server.arg("ssid"));
  //Serial.println(server.arg("pass"));
  grabar(50,server.arg("pass"));
  //Serial.println(server.arg("pass"));
  grabar(100,server.arg("user_e"));

  contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
  "<h3> Configuración guardada... </h3><br>"
  "<h3> Cambie el modo del ESP32-CAM y reinicielo </h3>"
  "</div><br>";

  server.send(200, "text/html", pagina_p1 + contenido + paginafin); 
}

//---------------- Función para GRABAR en la EEPROM -------------------
void grabar(int addr, String a) {
  int tamano = a.length(); 
  char inchar[50]; 

  a.toCharArray(inchar, tamano+1);
  for (int i = 0; i < tamano; i++) {
    EEPROM.write(addr+i, inchar[i]);
  }

  for (int i = tamano; i < 50; i++) {
    EEPROM.write(addr+i, 255);
  }

  EEPROM.commit();
}

//----------------- Función para LEER la EEPROM ------------------------
String leer(int addr) {
   byte lectura;
   String strlectura;
   for (int i = addr; i < addr+50; i++) {
      lectura = EEPROM.read(i);
      if (lectura != 255) {
        strlectura += (char)lectura;
      }
   }
   return strlectura;
}

//--------------------------- ESCANEAR REDES ----------------------------
void escanear() {  
  int n = WiFi.scanNetworks(); //devuelve el número de redes encontradas

  if (n == 0) { //si no encuentra ninguna red
    contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
        "<form action='guardar_conf' method='get' class = 'apartado flex_column'>"
          "<h3> Configuración de Punto de Acceso: </h3>"
          "<h3 style= 'width: 90%; text-align: left;'> * Ingresar SSID: </h3>" 
          "<input class='input1' name='ssid' type='text' autocomplete='off'>"

          "<h3 style= 'width: 90%; text-align: left;'> * Ingresar PASSWORD: </h3>"  
          "<input class='input1' name='pass' type = 'password' autocomplete='off'><br>"
          
          "<h3 style= 'width: 90%; text-align: left;'> * Ingresar un NOMBRE DE USUARIO: </h3>"  
          "<input class='input1' name='user_e' type = 'text' autocomplete='off' minlength=10><br>"

          "<input class='boton' type='submit' value = 'GUARDAR CONFIGURACIÓN'/><br>"
        "</form>"
      "</div><br>"

      "<div style= 'width: 100%'  class = 'flex_column' class = 'aartado flex_column'>"
          "<h3> No han sido encontradas redes disponibles </h3>"
          "<a href='escanear'>"
            "<button class='boton'> ESCANEAR REDES DISPONIBLES </button>"
          "</a>"
      "</div>";
  }else{
    contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
            "<form action='guardar_conf' method='get' class = 'apartado flex_column'>"
              "<h3> Configuración de Punto de Acceso: </h3>"
              "<h3 style= 'width: 90%; text-align: left;'> * Ingresar SSID: </h3>" 
              "<input class='input1' name='ssid' type='text' autocomplete='off'>"

              "<h3 style= 'width: 90%; text-align: left;'> * Ingresar PASSWORD: </h3>"  
              "<input class='input1' name='pass' type = 'password' autocomplete='off'><br>"
              
              "<h3 style= 'width: 90%; text-align: left;'> * Ingresar un NOMBRE DE USUARIO: </h3>"  
              "<input class='input1' name='user_e' type = 'text' autocomplete='off' minlength=10><br>"

              "<input class='boton' type='submit' value = 'GUARDAR CONFIGURACIÓN'/><br>"
            "</form>"
          "</div><br>"

          "<div style= 'width: 100%'  class = 'flex_column' class = 'aartado flex_column'>"
              "<h3> Lista de redes encontradas: </h3>";


    for (int i = 0; i < n; ++i){
      // agrega al STRING "contenido" la información de las redes encontradas
      contenido = (contenido) + "<p><b>" + String(i + 1) + ") " + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + ") </b><br><b>Canal:</b> " + WiFi.channel(i) + " <br><b>Tipo de cifrado:</b> " + WiFi.encryptionType(i) + " </p>\r\n";
      //WiFi.encryptionType 5:WEP 2:WPA/PSK 4:WPA2/PSK 7:open network 8:WPA/WPA2/PSK
      delay(10);
    }

    contenido = (contenido) + "<a href='escanear'>"
          "<button class='boton'> ESCANEAR REDES DISPONIBLES </button>"
        "</a>"
    "</div>";

    paginaconf();
  }
}


//------------------------SETUP WIFI-----------------------------
int setup_wifi() {
  // Conexión WIFI
  WiFi.mode(WIFI_STA); //para que no inicie el SoftAP en el modo normal
  //WiFi.begin("H@WI_521-B13R", "0Brg3@#._P1nXy?@");
  //WiFi.begin("INFINITUM7F36", "bXMBS345TR");
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED and contconexion < 50) { //Cuenta hasta 50 si no se puede conectar lo cancela
    //Indicando que esta buscando si existe una conexión establecida entre ESP32-CAM y el WIFI
    ++contconexion;
    delay(250);
    Serial.print(".");

    digitalWrite(conexion_internet, HIGH);
    delay(250);
    digitalWrite(conexion_internet, LOW);
  }
  Serial.println();
  if (contconexion < 50) {
      Serial.println("Conexión OK");
      digitalWrite(conexion_internet, HIGH); // LED INDICANDO QUE HAY CONEXIÓN CON WIFI (ENCENDIDO)
      return 1;
  } else { 
      contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
          "<h3 style= 'width: 90%'> Error en la conexión </h3>"    
          "<h3 style= 'width: 90%'> Intente volver a configurar el ESP32-cam </h3>"  
      "</div><br>";

      digitalWrite(conexion_internet, LOW); // LED INDICANDO QUE NO HAY CONEXIÓN CON WIFI (APAGADO)
      return 0;
  }
}

// Envio de foto a drive
void realizar_lectura() {
  saveCapturedImage();
}

//------------------------SETUP-----------------------------
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // Inicia Serial
  Serial.begin(115200);
  EEPROM.begin(512);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  /*if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {*/
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  //}

  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    pinMode(camara_activa, OUTPUT);  // LED INDICADOR DE CÁMARA ACTIVADA
    Serial.printf("Camera init failed with error 0x%x", err);
    digitalWrite(camara_activa, LOW);
    delay(1000);
    ESP.restart();
  }else{
    pinMode(camara_activa, OUTPUT);  // LED INDICADOR DE CÁMARA ACTIVADA
    digitalWrite(camara_activa, HIGH);
  }

  sensor_t * s = esp_camera_sensor_get();

  // Selectable camera resolution details :
  // -UXGA   = 1600 x 1200 pixels
  // -SXGA   = 1280 x 1024 pixels
  // -XGA    = 1024 x 768  pixels
  // -SVGA   = 800 x 600   pixels
  // -VGA    = 640 x 480   pixels
  // -CIF    = 352 x 288   pixels
  // -QVGA   = 320 x 240   pixels
  // -HQVGA  = 240 x 160   pixels
  // -QQVGA  = 160 x 120   pixels
  s->set_framesize(s, FRAMESIZE_VGA);  //--> UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
  s->set_special_effect(s, 0);
  s->set_brightness(s, -1);     // -2 to 2
  s->set_contrast(s, 2);       // -2 to 2

  pinMode(conexion_internet, OUTPUT); // LED INDICADOR DE CONEXIÓN CON WIFI
  pinMode(modo, INPUT);           // BOTÓN DE CONFIGURACIÓN
  pinMode(led_FLASH, OUTPUT);         
  
  // Si desea configurar el SSID Y PASS de Wifi - Debe dejar oprimido el botón al energizar el ESP32-CAM
  if(digitalRead(modo) == 0) { 
    modoconf();
  }

  // Al energizar al ESP32-CAM siempre realiza la lectura de la EEPROM
  leer(0).toCharArray(ssid, 50);
  leer(50).toCharArray(pass, 50);
  leer(100).toCharArray(user, 50);

  delay(1000);

  // Estableciendo conexión con el wifi
  banconection = setup_wifi();
}

//--------------------------LOOP--------------------------------
void loop(){
  if(banconection == 1){
    Serial.println(ssid);
    Serial.println(pass);
    Serial.println(user);

    if (!MDNS.begin("esp32cam")) {
      Serial.println("Error al iniciar mDNS");
      // Aquí puedes tomar alguna acción adicional según tus necesidades
    } else {
      Serial.println("mDNS iniciado correctamente");
      // Anunciar el servicio HTTP en el puerto 80
      MDNS.addService("http", "tcp", 80);
    }
    
    server.on("/realizar_lectura", realizar_lectura); //Graba en la eeprom la configuracion
    server.on("/consultar_lecturas", consulta_lects); // Solicita el historial de lecturas realizadas
    server.on("/", paginainicio); //esta es la pagina de configuracion
    server.begin();
  
    while(true){
      server.handleClient();
    }
  }else{
    while(true){
      //Indicando que esta buscando si existe una conexión establecida entre ESP32-CAM y el WIFI
      Serial.print("x");
      digitalWrite(conexion_internet, HIGH);
      digitalWrite(camara_activa, HIGH);
      delay(250);

      digitalWrite(conexion_internet, LOW);
      digitalWrite(camara_activa, LOW);
      delay(250);
    }
  }
}