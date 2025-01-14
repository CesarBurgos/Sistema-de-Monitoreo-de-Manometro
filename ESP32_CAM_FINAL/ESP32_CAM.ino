#include "WebServer.h"
#include "WiFi.h"
#include "esp32cam.h"
#include "esp_camera.h"
#include <ESPmDNS.h>
#include <EEPROM.h>

#define led_FLASH 4
#define conexion_internet 13
#define camara_activa 12

//-------------------VARIABLES GLOBALES--------------------------
int contconexion = 0;
unsigned long previousMillis = 0;

char ssid[50];      
char pass[50];

const char *ssidConf = "esp32cam";
const char *passConf = "12345678";

const uint32_t TiempoEsperaWifi = 5000;

unsigned long TiempoActual = 0;
unsigned long TiempoAnterior = 0;
const long TiempoCancelacion = 500;

int banconection = 0; // Bandera que indicará si esta conectado

String mensaje = "";

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
      "height: 100%;"
    "}"

    "h1, h2, h3, h4{"
      "font-family: 'Segoe UI', Tahoma, Verdana, sans-serif;"
      "margin-bottom: 0;"
    "}"

    "input, p{"
      "width: 90%;"
      "font-family: 'Segoe UI', Tahoma, Verdana, sans-serif;"
    "}"

    ".contenido{"
      "background-color: rgb(233, 233, 233);"
      "width: 95%;"
      "height: 100%;"
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
      "background-color: rgb(223, 240, 252);"
    "}"

    ".boton:hover{"
      "box-shadow: 2px 2px 2px gray;"
    "}"

    "footer h4{"
      "color: white;"
    "}"
  "</style>"
  "<body class = 'flex_column'>"
      "<div class = 'contenido flex_column'>"
      "<h3> Digitalización de información de un manómetro analógico a través de visión artificial </h3><br>";

String paginafin = "</div><br>"
    "<footer class = 'flex_column'>"
      "<h4>Website developed by Cesar Brgs. (2023)</h4>"
    "</footer>"
  "</body>"
"</html>";

// ** Configuración
String contenido;
/*String contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
        "<form action='guardar_conf' method='get' class = 'apartado flex_column'>"
          "<h3> Configuración de Punto de Acceso: </h3>"
          "<h3 style= 'width: 90%'> * Ingresar SSID: </h3>" 
          "<input class='input1' name='ssid' type='text' autocomplete='off'>"

          "<h3 style= 'width: 90%'> * Ingresar PASSWORD: </h3>"  
          "<input class='input1' name='pass' type = 'password' autocomplete='off'><br>"
          "<input class='boton' type='submit' value = 'GUARDAR CONFIGURACIÓN'/><br>"
        "</form>"
      "</div><br>"

      "<a href='escanear'>"
        "<button class='boton'> ESCANEAR REDES DISPONIBLES </button>"
      "</a>";*/

String pagina2 = "<!DOCTYPE html>"
"<html>"
  "<head>"
  "<title>ESP32-CAM (Manometro)</title>"
  "<meta charset='UTF-8'>"
  "</head>"
"<body>"

"<a href='https://www.youtube.com'>"
  "<button class='boton'> Soy un botón </button>"
"</a><br><br>";



//--------------------------------------------------------------
WebServer server(80);
//--------------------------------------------------------------

static auto loRes = esp32cam::Resolution::find(320, 240); //baja resolucion
static auto hiRes = esp32cam::Resolution::find(800, 600); //alta resolucion 

void serveJpg(){ //captura imagen .jpg
  auto frame = esp32cam::capture();
  digitalWrite(led_FLASH, LOW); 
  if (frame == nullptr) {
    
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  
  
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  
  WiFiClient client = server.client();
  Serial.print(client);
  frame->writeTo(client);  // y envia a un cliente (en este caso sera python)
}

void handleJpgLo(){  //permite enviar la resolucion de imagen baja
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  digitalWrite(led_FLASH, HIGH);
  delay(1000);
  serveJpg();
}

void handleJpgHi(){ //permite enviar la resolucion de imagen alta
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  digitalWrite(led_FLASH, HIGH);
  delay(1000);
  serveJpg();
}



//-------------------PAGINA DE CONFIGURACION--------------------
void paginaconf() {
  server.send(200, "text/html", pagina_p1 + contenido + paginafin); 
}

void paginainicio() {
  server.send(200, "text/html", pagina_p1 + mensaje + paginafin); 
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
          "<h3 style= 'width: 90%'> * Ingresar SSID: </h3>" 
          "<input class='input1' name='ssid' type='text' autocomplete='off'>"

          "<h3 style= 'width: 90%'> * Ingresar PASSWORD: </h3>"  
          "<input class='input1' name='pass' type = 'password' autocomplete='off'><br>"
          "<input class='boton' type='submit' value = 'GUARDAR CONFIGURACIÓN'/><br>"
        "</form>"
      "</div><br>"

      "<a href='escanear'>"
        "<button class='boton'> ESCANEAR REDES DISPONIBLES </button>"
      "</a>";

  WiFi.softAP(ssidConf, passConf);
  IPAddress myIP = WiFi.softAPIP(); 
  //Serial.print("IP del acces point: ");
  //Serial.println(myIP);
  //Serial.println("WebServer iniciado...");

  server.on("/", paginaconf);               // Página de configuración
  server.on("/guardar_conf", guardar_conf); // Graba en la eeprom la configuración
  server.on("/escanear", escanear);         // Escanean las redes WIFI disponibles
  server.begin();

  if (!MDNS.begin("esp32cam")) {
    //Serial.println("Error configurando mDNS!");
    while(1){
      delay(1000);
    }
  }
  //Serial.println("mDNS configurado");
  MDNS.addService("http", "tcp", 80);

  while (true) {
      server.handleClient();
  }
}

//---------------------GUARDAR CONFIGURACION-------------------------
void guardar_conf() {
  //Serial.println(server.arg("ssid"));//Recibimos los valores que envia por GET el formulario web
  grabar(0,server.arg("ssid"));
  //Serial.println(server.arg("pass"));
  grabar(50,server.arg("pass"));

  contenido = "Configuracion Guardada...";

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

//---------------------------ESCANEAR----------------------------
void escanear() {  
  int n = WiFi.scanNetworks(); //devuelve el número de redes encontradas
  //Serial.println("escaneo terminado");
  if (n == 0) { //si no encuentra ninguna red
    //Serial.println("no se encontraron redes");
    contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
        "<form action='guardar_conf' method='get' class = 'apartado flex_column'>"
          "<h3> Configuración de Punto de Acceso: </h3>"
          "<h3 style= 'width: 90%'> * Ingresar SSID: </h3>" 
          "<input class='input1' name='ssid' type='text' autocomplete='off'>"

          "<h3 style= 'width: 90%'> * Ingresar PASSWORD: </h3>"  
          "<input class='input1' name='pass' type = 'password' autocomplete='off'><br>"
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
    //Serial.print(n);
    //Serial.println(" redes encontradas");
    contenido = "<div style= 'width: 100%'  class = 'flex_column'>"
            "<form action='guardar_conf' method='get' class = 'apartado flex_column'>"
              "<h3> Configuración de Punto de Acceso: </h3>"
              "<h3 style= 'width: 90%'> * Ingresar SSID: </h3>" 
              "<input class='input1' name='ssid' type='text' autocomplete='off'>"

              "<h3 style= 'width: 90%'> * Ingresar PASSWORD: </h3>"  
              "<input class='input1' name='pass' type = 'password' autocomplete='off'><br>"
              "<input class='boton' type='submit' value = 'GUARDAR CONFIGURACIÓN'/><br>"
            "</form>"
          "</div><br>"

          "<div style= 'width: 100%'  class = 'flex_column' class = 'aartado flex_column'>"
              "<h3> Lista de redes encontradas: </h3>";


    for (int i = 0; i < n; ++i){
      // agrega al STRING "mensaje" la información de las redes encontradas
      contenido = (contenido) + "<p><b>" + String(i + 1) + ") " + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + ") </b><br><b>Canal:</b> " + WiFi.channel(i) + " <br><b>Tipo de cifrado:</b> " + WiFi.encryptionType(i) + " </p>\r\n";
      //WiFi.encryptionType 5:WEP 2:WPA/PSK 4:WPA2/PSK 7:open network 8:WPA/WPA2/PSK
      delay(10);
    }

    contenido = (contenido) + "<a href='escanear'>"
          "<button class='boton'> ESCANEAR REDES DISPONIBLES </button>"
        "</a>"
    "</div>";

    //Serial.println(mensaje);
    paginaconf();
  }
}


//------------------------SETUP WIFI-----------------------------
int setup_wifi() {
  // Conexión WIFI
  WiFi.mode(WIFI_STA); //para que no inicie el SoftAP en el modo normal
  WiFi.begin(ssid, pass);

  Serial.println(ssid);
  Serial.println(pass);

  while (WiFi.status() != WL_CONNECTED and contconexion < 50) { //Cuenta hasta 50 si no se puede conectar lo cancela
    //Indicando que esta buscando si existe una conexión establecida entre ESP32-CAM y el WIFI
    ++contconexion;
    delay(250);
    //Serial.print(".");
    digitalWrite(conexion_internet, HIGH);
    delay(250);
    digitalWrite(conexion_internet, LOW);
  }

  if (contconexion < 50) {
      //Serial.println("");
      //Serial.println("WiFi conectado");
      //server.begin();

      //Aca debe de enviar este valor de la IP a algun lado... CREO QUE DEBO GUARDARLA EN UNA VARIABLE
      //Serial.println(WiFi.localIP());
      digitalWrite(conexion_internet, HIGH); // LED INDICANDO QUE HAY CONEXIÓN CON WIFI (ENCENDIDO)
      return 1;
  } else { 
      //Serial.println("");
      //Serial.println("Error de conexion");
      digitalWrite(conexion_internet, LOW); // LED INDICANDO QUE NO HAY CONEXIÓN CON WIFI (APAGADO)
      return 0;
  }
}


//------------------------SETUP-----------------------------
void setup() {
  // Inicia Serial
  Serial.begin(115200);
  Serial.println("");
  EEPROM.begin(512);

  
  using namespace esp32cam;
  Config cfg;
  cfg.setPins(pins::AiThinker);
  cfg.setResolution(hiRes);
  cfg.setBufferCount(2);
  cfg.setJpeg(80);
  bool conf_cam = Camera.begin(cfg);
  pinMode(camara_activa, OUTPUT);  // LED INDICADOR DE CÁMARA ACTIVADA
  if(conf_cam){
    Serial.print("Cámara OK ");
    digitalWrite(camara_activa, HIGH);
  }else{
    Serial.print("Cámara NO ");
    digitalWrite(camara_activa, LOW);
  }


  pinMode(conexion_internet, OUTPUT); // LED INDICADOR DE CONEXIÓN CON WIFI
  pinMode(14, INPUT);  // BOTÓN DE CONFIGURACIÓN
  pinMode(led_FLASH, OUTPUT);
  
  // Si desea configurar el SSID Y PASS de Wifi - Debe dejar oprimido el botón al energizar el ESP32-CAM
  if(digitalRead(14) == 0) { 
    modoconf();
  }

  // Al energizar al ESP32-CAM siempre realiza la lectura de la EEPROM
  leer(0).toCharArray(ssid, 50);
  leer(50).toCharArray(pass, 50);

  // Estableciendo conexión con el wifi
  banconection = setup_wifi();
}

//--------------------------LOOP--------------------------------
void loop() {
  if (banconection == 1){
    IPAddress myIP = WiFi.softAPIP(); 
    Serial.print("IP del acces point: ");
    Serial.println(myIP);
    Serial.println("WebServer iniciado...");

    Serial.println(WiFi.localIP());

    Serial.print("http://");
    Serial.print(WiFi.localIP());
    Serial.println("/cam-lo.jpg");//para conectarnos IP res baja

    Serial.print("http://");
    Serial.print(WiFi.localIP());
    Serial.println("/cam-hi.jpg");//para conectarnos IP res alta

    server.on("/cam-lo.jpg", handleJpgLo);//enviamos al servidor
    server.on("/cam-hi.jpg", handleJpgHi);

    mensaje = String("http://") + WiFi.localIP().toString() + "/cam-hi.jpg \r\n" + WiFi.softAPIP().toString();

    server.on("/", paginainicio); //esta es la pagina de configuracion
    server.on("/guardar_conf", guardar_conf); //Graba en la eeprom la configuracion
    server.on("/escanear", escanear); //Escanean las redes wifi disponibles
    


    server.begin();

    if (!MDNS.begin("esp32cam")) {
      Serial.println("Erro configurando mDNS!");
      while (1) {
        delay(1000);
      }
    }
    Serial.println("mDNS configurado");
    MDNS.addService("http", "tcp", 80);

    while (true) {
      server.handleClient();
    }
  }
    
}