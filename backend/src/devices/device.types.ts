export type DeviceStatus = 'ONLINE';

export interface NetworkDevice {
  id: string;
  hostname: string;
  ipv4: string;
  mac: string;
  vendor: string;
  type: string;
  status: DeviceStatus;
}

export interface DeviceScanResponse {
  network: string;
  scannedAt: string;
  devices: NetworkDevice[];
}
