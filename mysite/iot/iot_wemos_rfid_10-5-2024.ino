/*
    FACULDADE SENAC PE
    Baseado em Projeto PET Capotaria Batista 2022
    Autor: Prof. Arnott Ramos Caiado
    Estrutura Exemplo de envio de dados para APIs HTTP
    versao com uso de ID e KEY
    Hardware : wemos ESP32 D1
    Data: 13-6-2023
    Atualizado: 1-5-2024

    Placa Wemos: deve ser configurada como WEMOS D1 MINI ESP32

    https://www.filipeflop.com/blog/como-utilizar-o-sensor-de-gestos-e-rgb-sparkfun/
*/
/*
 * This ESP32 code is created by esp32io.com
 *
 * This ESP32 code is released in the public domain
 *
 * For more detail (instruction and wiring diagram), visit https://esp32io.com/tutorials/esp32-rfid-nfc
 */

// #include <ESP8266HTTPClient.h>
#include <HTTPClient.h>
#include <WiFiMulti.h>

#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>


#ifdef __AVR__
  #include <avr/power.h>
#endif

#define ETAPA "05"  // deixar claro aqui qual a etapa onde o modulo esta instalado

#define NTENTS_ENVIO 4

#define STAND_BY      0
#define INICIO        1
#define ERRO_WIFI     2
#define ERRO_LEITURA  3
#define ERRO_ENVIO    4
#define ERRO_CARTAO   5
#define ENVIO_OK      6
#define CARTAO_LIDO   7
#define ERRO_AUTH     8  // erro de autenticacao no envio
#define ERROMSG "ERR"
#define ERROMSG_AUTH "ERR AUTH"

#define ENVIO_ITEM "ITEM"   // mensagem de retorno caso item seja lido ok
#define ENVIO_COLAB "COLAB" // mensagem de retorno caso cartao de colab seja lido ok
#define ENVIO_OS "OS"       // mensagem de retorno caso os seja lida ok

#define AMARELO 0x002200  // FFFF00
#define VERMELHO 0x220000 // FF0000
#define VERDE 0x002200    // 00FF00
#define AZUL 0x0000FF
#define ROSA 0x401310
#define MOSTARDA 0xF38000 // FF7F00

WiFiClient client;
HTTPClient http;

#define NWIFIS 6

struct redes  {
  const char *ssid;
  const char *senha;
};

char *redeAtual;

/*
struct redes redeWifi[NWIFIS]={ "Vivo-Internet-E532", "6EFC366C",
                          "SENAC-Mesh","09080706",
                          "AP1501","ARBBBE11",
                          "iPhone de Arnott","arbbbe11",
                          "SENAC-MESH","09080706",
                          "NET_2G6E0F85", "Prod@2020#CB",
                          "Vivo-Internet-E532", "6EFC366C",
                          "iPhone de Arnott","arbbbe11"};
*/


struct redes redeWifi[NWIFIS]={"AP1501","ARBBBE11",
                          "iPhone de Arnott","arbbbe11",
                          "SENAC-Mesh","09080706",
                          "Vivo-Internet-E532", "6EFC366C",
                          "iPhone de Arnott","arbbbe11",
                          "SENAC-MESH","09080706"};

// SDA/SS - D8
// SCK - D5
// MOSI - D7
// MISO - D6
// RST - D3

// LED - D4

// pinagem wemos d1 r32
// SDA/SS   - GIOP 5
// SCK      - GIOP 18
// MOSI     - GIOP 23
// MISO     - GIOP 19
// RST      - GIOP 27


constexpr uint8_t RST_PIN = 27; // pino RST 22 e 24;  d3 no wemos 8266
constexpr uint8_t SS_PIN = 5; // pino SDA 20; // d8

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

String cartao = "";
int estado = 0;
int respostaHttp = 0;
bool conectou = false;

#define TIPODADO "application/json"
#define api_header_key  "7312f2e18eb95dea413baf7f321c98e3fa27dbb3"
#define http_post "http://petcb.pythonanywhere.com/monitoraOsJson"  // url da API
#define IDMODULO "IDW3"           // definição da identificação do modulo de leitura e placa
#define KEYAPI "ADS2NKEY01"       // definição da chave de segurança - deve ser criptografada e sofrer mudanças temporárias

void setup()
{

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW

  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  estado = INICIO;
  delay(1000);

  do{
      conectou = conectaWifi(); // tenta conectar em uma das redes disponiveis
      if ( !conectou )
        delay(100);

  }while(!conectou);

 Serial.println("Setup concluido");
 estado = STAND_BY;
}


void loop()
{
    estado = STAND_BY;
    cartao = lerCartao();
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)

    Serial.print("-");
    delay(200);

    if(cartao == "x"){
       estado = STAND_BY;
    }
    else if ( cartao != "" ){
      Serial.println(" ");
      Serial.println("Cartao Lido:");
      Serial.println(cartao);
      MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
      Serial.print("RFID/NFC Tag Type: ");
      Serial.println(rfid.PICC_GetTypeName(piccType));

      estado = CARTAO_LIDO;
      delay(200);

      for ( int i = 0; i < NTENTS_ENVIO ; i++){
        if(WiFi.status() == WL_CONNECTED){
            estado = enviaDados(ETAPA, cartao, http_post, TIPODADO, IDMODULO, KEYAPI);
            delay(300);
            if ( estado == ENVIO_OK ){
              delay(500);
              break;
            } else if ( estado == ERRO_CARTAO ){
              delay(1000);
              break;
            }
        } else { // problemas de wifi
            estado = ERRO_WIFI ;
            delay(500);
        }
      }
    }
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
}


//------------------------------------------------------------------------------------
bool conectaWifi()
{
  bool conectou = false;
  int i=0;
  int qtd = 0;

  while ( i < NWIFIS ){ // tenta cada uma das redes disponiveis

      Serial.println ("Conectando: wifi");
      Serial.println( redeWifi[i].ssid );
      WiFi.begin(redeWifi[i].ssid, redeWifi[i].senha);

      qtd = 0;

      while (WiFi.status() != WL_CONNECTED) {
        delay(400);
        Serial.print(".");
        qtd++;
        if(qtd > 30){
          break;
        }
      }
      if(WiFi.status() == WL_CONNECTED){
        Serial.println("Conectado com sucesso. IP: ");
        Serial.println(WiFi.localIP());
        Serial.println( redeWifi[i].ssid );
        delay( 2000 );
        conectou = true;
        redeAtual = (char *)redeWifi[i].ssid;

        return conectou;
      }
      i++;
    }
  return conectou;
}

//--------------------------------------------------------------------------------------
int enviaDados(String etapa, String card, String url, String tipoDado, String id_modulo, String chave_modulo)
{
  String dados = "";
  char temp[255];
  int retorno = 0, cont = 0;
  DynamicJsonDocument doc(150);
  http.begin(client, url);
  http.addHeader("Content-Type", tipoDado);
  http.addHeader("Authorization-Token", api_header_key );

  // acrescentar chave de autenciacao no header
  // http.addHeader("Authentication-key", keyheader );
  doc["etapa"] = etapa;
  doc["cartao"] = card;
  doc["api_key"] = chave_modulo;
  doc["id_modulo"] = id_modulo;

  serializeJson(doc, dados);
  Serial.println( dados );
  retorno = http.POST(dados);
  delay(200);

  if(retorno > 0){ // envio bem sucedido
    if ( retorno <= 299 ){ // requisicao ok
      String resposta = http.getString();
      Serial.println(retorno);
      Serial.println(resposta);
      Serial.println( redeAtual );

      resposta.toCharArray( temp, resposta.length()+1 );
      estado = ENVIO_OK;
      if ( strcmp( temp , ERROMSG ) == 0 ){
        Serial.println("Cartao invalido");
        estado = ERRO_CARTAO ;
      }
      if ( strcmp( temp , ERROMSG_AUTH ) == 0 ){
        Serial.println("Falha de autenticacao");
        estado = ERRO_AUTH ;
      }
    } else {
      Serial.println("Erro https");
      estado = ERRO_ENVIO;
    }
  }else{ // erro na requisicao
    Serial.print("Erro na requisição:");
    Serial.println( url );
    estado = ERRO_ENVIO;
  }
  http.end();
  return estado;
}
//--------------------------------------------------------------------------------------
String lerCartao()
{
  String card = "";
  if (rfid.PICC_IsNewCardPresent())
  {
    card="x";
    if (rfid.PICC_ReadCardSerial())
    {
      card = "";
      for (byte i = 0; i < rfid.uid.size; i++)
      {
         card.concat(String(rfid.uid.uidByte[i] < 0x10 ? "0" : ""));
         card.concat(String(rfid.uid.uidByte[i], HEX));
      }
      card.toUpperCase();
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
    return card;
  }
}