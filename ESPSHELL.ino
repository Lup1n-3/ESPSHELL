#include "BLEDevice.h"

// Pines correctos para los joysticks y botones
const int joystickMovePin = 36; // Eje de movimiento (Adelante/Atrás)
const int joystickTurnPin = 39; // Eje de giro (Izquierda/Derecha)
const int turboPin = 33;        // Botón para activar el modo turbo
const int ledPin = 25;          // LED indicador de conexión

// Brandbase Car BLE UUIDs
static BLEUUID serviceUUID_Brandbase("0000fff0-0000-1000-8000-00805f9b34fb");
static BLEUUID charUUID_Brandbase("d44bc439-abfd-45a2-b575-925416129600");

// Comandos BLE
uint8_t IDLE_COMMAND[] = {0x02, 0x5e, 0x69, 0x5a, 0x48, 0xff, 0x2a, 0x43, 0x8c, 0xa6, 0x80, 0xf8, 0x3e, 0x04, 0xe4, 0x5d};
uint8_t FORWARD_COMMAND[] = {0x29, 0x60, 0x9c, 0x66, 0x48, 0x52, 0xcf, 0xf1, 0xb0, 0xf0, 0xcb, 0xb9, 0x80, 0x14, 0xbd, 0x2c};
uint8_t FORWARD_TURBO_COMMAND[] = {0xe6, 0x55, 0x67, 0xda, 0x8e, 0x6c, 0x56, 0x0d, 0x09, 0xd3, 0x73, 0x3a, 0x7f, 0x47, 0xff, 0x06};
uint8_t BACKWARD_COMMAND[] = {0x03, 0x20, 0x99, 0x09, 0xba, 0x9d, 0xa1, 0xc8, 0xb9, 0x86, 0x16, 0x3c, 0x6d, 0x48, 0x46, 0x55};
uint8_t LEFT_COMMAND[] = {0x51, 0x38, 0x21, 0x12, 0x13, 0x5c, 0xcc, 0xdb, 0x46, 0xcf, 0x89, 0x21, 0xb7, 0x05, 0x49, 0x9a};
uint8_t RIGHT_COMMAND[] = {0x1b, 0x57, 0x69, 0xcd, 0xf1, 0x3e, 0x8a, 0xb6, 0x27, 0x08, 0x0f, 0xf3, 0xce, 0xfc, 0x3b, 0xc0};

static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

// Valores del joystick
int moveValue = 0;  // Adelante/Atrás
int turnValue = 0;  // Izquierda/Derecha

// Valores neutros y umbrales
const int neutralMove = 2036; // Neutro del joystick de movimiento
const int neutralTurn = 2047; // Neutro del joystick de giro
const int deadZone = 800;     // Zona muerta para evitar movimientos indeseados

bool turboMode = false; // Estado del modo turbo
bool lastTurboButtonState = HIGH; // Estado anterior del botón turbo (con pull-up)

// Callback para encontrar dispositivos BLE
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.print("Dispositivo encontrado: ");
        Serial.println(advertisedDevice.toString().c_str());

        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID_Brandbase)) {
            Serial.println("Dispositivo compatible encontrado, deteniendo escaneo.");
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
        }
    }
};

bool connectToServer() {
    Serial.print("Intentando conectar con: ");
    Serial.println(myDevice->getAddress().toString().c_str());

    BLEClient* pClient = BLEDevice::createClient();
    Serial.println(" - Cliente BLE creado.");

    if (!pClient->connect(myDevice)) {
        Serial.println("Fallo al conectar al servidor BLE.");
        return false;
    }

    Serial.println(" - Conectado al servidor BLE.");
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID_Brandbase);
    if (pRemoteService == nullptr) {
        Serial.println("Fallo al encontrar el servicio BLE.");
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Servicio encontrado.");

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_Brandbase);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Fallo al encontrar la característica BLE.");
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Característica encontrada.");

    connected = true;
    digitalWrite(ledPin, HIGH); // Enciende el LED al conectar
    return true;
}

void sendCommand(uint8_t* command, int length) {
    if (connected) {
        pRemoteCharacteristic->writeValue(command, length);
        Serial.println("Comando enviado.");
    } else {
        Serial.println("No conectado al dispositivo BLE.");
    }
}

void handleTurboButton() {
    bool turboButtonState = digitalRead(turboPin);

    // Detecta un cambio de estado del botón
    if (turboButtonState == LOW && lastTurboButtonState == HIGH) { // Botón presionado
        turboMode = !turboMode; // Cambia el estado del modo turbo
        Serial.println(turboMode ? "Modo Turbo ACTIVADO" : "Modo Turbo DESACTIVADO");
    }

    // Actualiza el estado anterior del botón
    lastTurboButtonState = turboButtonState;
}

void readJoystick() {
    moveValue = analogRead(joystickMovePin);
    turnValue = analogRead(joystickTurnPin);

    Serial.print("Move Value (Forward/Backward): ");
    Serial.print(moveValue);
    Serial.print(" | Turn Value (Left/Right): ");
    Serial.println(turnValue);

    // Determina qué comando enviar basado en los valores del joystick
    if (abs(moveValue - neutralMove) > deadZone || abs(turnValue - neutralTurn) > deadZone) {
        if (moveValue > neutralMove + deadZone) {
            sendCommand(turboMode ? FORWARD_TURBO_COMMAND : FORWARD_COMMAND, sizeof(FORWARD_COMMAND));
        } else if (moveValue < neutralMove - deadZone) {
            sendCommand(BACKWARD_COMMAND, sizeof(BACKWARD_COMMAND));
        } else if (turnValue > neutralTurn + deadZone) {
            sendCommand(RIGHT_COMMAND, sizeof(RIGHT_COMMAND));
        } else if (turnValue < neutralTurn - deadZone) {
            sendCommand(LEFT_COMMAND, sizeof(LEFT_COMMAND));
        }
    } else {
        // Dentro de la zona muerta, enviar comando IDLE
        sendCommand(IDLE_COMMAND, sizeof(IDLE_COMMAND));
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Iniciando controlador BLE...");
    pinMode(ledPin, OUTPUT);
    pinMode(turboPin, INPUT_PULLUP); // Botón de modo turbo

    BLEDevice::init("");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);

    Serial.println("Iniciando escaneo BLE...");
    pBLEScan->start(5, false);
}

void loop() {
    if (doConnect && !connected) {
        if (connectToServer()) {
            Serial.println("Conexión exitosa al servidor BLE.");
        } else {
            Serial.println("Conexión fallida, reiniciando escaneo.");
            BLEDevice::getScan()->start(0);  // Reinicia el escaneo si falla la conexión
        }
        doConnect = false;
    }

    handleTurboButton(); // Maneja el estado del botón de turbo
    readJoystick();      // Lee los valores del joystick y envía los comandos

    delay(100); // Ajustar según la frecuencia deseada
}
