#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Definição dos pinos para o ESP32
#define SS_PIN 5    // Pino do ESP32 conectado ao pino SDA do MFRC522
#define RST_PIN 15  // Pino do ESP32 conectado ao pino RST do MFRC522

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Cria o objeto MFRC522

// Configurações de rede WiFi
const char* ssid = "Vicente Wifi";
const char* password = "98719885";
const char* server = "www.geaf-web.com.br";  // Nome do host
const char* resource = "/receber_rfi";       // Caminho do recurso
const int porta = 80;                        // Porta do servidor HTTP

const int relePin = 25;  // Pino do relé

WiFiClient client;

// Inicialização do display LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);      // Inicializa a comunicação serial
  SPI.begin();               // Inicializa a interface SPI
  mfrc522.PCD_Init();        // Inicializa o leitor RFID

  pinMode(relePin, OUTPUT);  // Configura o pino do relé como saída
 

  // Inicialização do display LCD
  lcd.init();
  lcd.backlight();  // Ativa a luz de fundo do LCD
  lcd.setCursor(0, 0);
  lcd.print("Conectando...");

  WiFi.begin(ssid, password);  // Conecta-se à rede WiFi

  while (WiFi.status() != WL_CONNECTED) {  // Aguarda a conexão
    delay(500);
    Serial.println("Conectando ao WiFi...");
  }
  Serial.println("Conectado ao WiFi!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  lcd.setCursor(0, 0);
  lcd.print("Posicione");
  lcd.setCursor(0, 1);
  lcd.print("o Cartao");

  // Verifica se há novas tags RFID presentes
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String rfidData = "";  // Armazena o código RFID lido

    for (byte i = 0; i < mfrc522.uid.size; i++) {
      rfidData.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
      rfidData.concat(String(mfrc522.uid.uidByte[i], HEX));
    }
    Serial.println("RFID lido: " + rfidData);

    // Exibe a mensagem no LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Consultando...");

    if (client.connect(server, porta)) {
      // Envia os dados RFID e o endereço IP local para o servidor HTTP
      String requestData = String("GET ") + resource + "/" + rfidData + "?ip=" + WiFi.localIP().toString() + " HTTP/1.1\r\n" +
                           "Host: " + server + "\r\n" +
                           "Connection: close\r\n\r\n";
      client.print(requestData);
      Serial.println("RFID e endereço IP enviados para o servidor");

      // Aguarda a resposta do servidor
      String response = "";
      while (client.connected() && !client.available()) {
        delay(1);  // Aguarda até que os dados estejam disponíveis
      }

      while (client.connected() || client.available()) {
        if (client.available()) {
          response += client.readStringUntil('\r');
        }
      }

      // Encontra o índice do início do corpo da resposta
      int bodyStartIndex = response.indexOf("\r\n\r\n") + 4;

      // Extrai o corpo da resposta
      String responseBody = response.substring(bodyStartIndex);

      // Verifica se a mensagem recebida contém "AUTORIZADO" ou "NEGADO"
      int autorizadoIndex = responseBody.indexOf("AUTORIZADO");
      int negadoIndex = responseBody.indexOf("NEGADO");

      if (autorizadoIndex != -1) {
        // Extrai o nome e sobrenome da pessoa autorizada
        int nomeEndIndex = responseBody.lastIndexOf(' ', autorizadoIndex - 1);
        int nomeStartIndex = responseBody.lastIndexOf(' ', nomeEndIndex - 1);
        String nomeAutorizado = responseBody.substring(nomeStartIndex + 8, nomeEndIndex);
        String sobrenomeAutorizado = responseBody.substring(nomeEndIndex + 1, responseBody.indexOf("-", nomeEndIndex + 1));

        // Extrai o texto "ASO VENC 11/05" após a palavra "AUTORIZADO"
        int asoIndex = responseBody.indexOf("ASO VENC", autorizadoIndex);
        String textoASO = responseBody.substring(asoIndex);

        // Exibe o nome da pessoa autorizada e "ASO VENC 11/05"
        Serial.println("Acesso autorizado para: " + nomeAutorizado);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(nomeAutorizado);
        lcd.print(" ");
        lcd.print(sobrenomeAutorizado);
        lcd.setCursor(0, 1);
        lcd.print(textoASO);
        delay(2000);  // Aguarda 2 segundos
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Autorizado");
        digitalWrite(relePin, HIGH);  // Ativa o relé
        delay(2000);                  // Mantém o relé ativado por 2 segundos
        digitalWrite(relePin, LOW);   // Desativa o relé
        lcd.clear();
      } else if (negadoIndex != -1) {
        // Extrai o nome da pessoa antes da palavra "NEGADO"
        int nomeStartIndex = responseBody.indexOf("-") + 1;
        int nomeEndIndex = responseBody.indexOf("-", nomeStartIndex);
        String nomeNegado = responseBody.substring(nomeStartIndex, nomeEndIndex);

        Serial.println("Acesso negado para: " + nomeNegado);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Acesso Negado:");
        lcd.setCursor(0, 1);
        lcd.print("Procurar RH");
        delay(1000);
        lcd.clear();
      } else {
        Serial.println("Acesso negado (sem resposta do servidor)");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Acesso Negado:");
        lcd.setCursor(0, 1);
        lcd.print("nao encontrada");
        delay(1000);
        lcd.clear();
      }
    } else {
      Serial.println("Falha ao conectar-se ao servidor");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Erro!");
    }

    delay(1000);  // Aguarda um pouco antes de ler o próximo cartão
  }
}
