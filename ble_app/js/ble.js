// Kết nối Nordic UART Service của Hộp Đen qua Web Bluetooth.
// UUID phải khớp Esp32_board/ble_app_config.h.

const NUS_SERVICE = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_RX = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // App -> ESP32 (write)
const NUS_TX = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // ESP32 -> App (notify)
const DEVICE_NAME_PREFIX = 'HOP_DEN';

export function isBluetoothSupported() {
  return typeof navigator !== 'undefined' && 'bluetooth' in navigator;
}

/**
 * @param {{ onBytes: (b: Uint8Array) => void, onDisconnect: () => void }} handlers
 */
export function createBlackboxLink({ onBytes, onDisconnect }) {
  let device = null;
  let rxChar = null;
  let writeChain = Promise.resolve();

  const handleNotify = (event) => {
    const view = event.target.value;
    onBytes(new Uint8Array(view.buffer, view.byteOffset, view.byteLength));
  };

  const handleDisconnect = () => {
    rxChar = null;
    onDisconnect();
  };

  async function openGatt() {
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(NUS_SERVICE);
    const txChar = await service.getCharacteristic(NUS_TX);
    rxChar = await service.getCharacteristic(NUS_RX);
    txChar.addEventListener('characteristicvaluechanged', handleNotify);
    await txChar.startNotifications();
  }

  return {
    get deviceName() { return device?.name ?? null; },
    get isConnected() { return Boolean(device?.gatt?.connected && rxChar); },

    async connect() {
      device = await navigator.bluetooth.requestDevice({
        filters: [{ namePrefix: DEVICE_NAME_PREFIX }, { services: [NUS_SERVICE] }],
        optionalServices: [NUS_SERVICE],
      });
      device.addEventListener('gattserverdisconnected', handleDisconnect);
      await openGatt();
    },

    // Kết nối lại thiết bị đã chọn, không cần hộp thoại chọn thiết bị
    async reconnect() {
      if (!device) throw new Error('Chưa chọn thiết bị nào');
      await openGatt();
    },

    disconnect() {
      if (device?.gatt?.connected) device.gatt.disconnect();
    },

    // Ghi tuần tự: Web Bluetooth từ chối 2 thao tác GATT chạy song song
    send(command) {
      const bytes = new TextEncoder().encode(command);
      const run = async () => {
        if (!rxChar) throw new Error('Chưa kết nối Hộp Đen');
        await rxChar.writeValueWithResponse(bytes);
      };
      const result = writeChain.then(run, run);
      writeChain = result.catch(() => {});
      return result;
    },
  };
}
