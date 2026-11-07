import {
  BadGatewayException,
  GatewayTimeoutException,
  Injectable,
  ServiceUnavailableException,
} from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { XMLParser } from 'fast-xml-parser';
import type { DeviceScanResponse, NetworkDevice } from './device.types';

const execFileAsync = promisify(execFile);
const MAX_SCAN_DURATION_MS = 30_000;

interface NmapAddress {
  addr?: string;
  addrtype?: string;
  vendor?: string;
}

interface NmapHost {
  address?: NmapAddress | NmapAddress[];
  hostnames?: {
    hostname?: { name?: string } | { name?: string }[];
  };
  status?: { state?: string };
}

interface NmapOutput {
  nmaprun?: { host?: NmapHost | NmapHost[] };
}

@Injectable()
export class DevicesService {
  private readonly parser = new XMLParser({
    ignoreAttributes: false,
    attributeNamePrefix: '',
  });

  constructor(private readonly configService: ConfigService) {}

  async scanNetwork(): Promise<DeviceScanResponse> {
    const network = this.configService.get<string>('LAN_SCAN_CIDR');
    this.validateNetwork(network);

    let xml: string;

    try {
      const result = await execFileAsync(
        'nmap',
        ['-sn', '-oX', '-', network],
        {
          timeout: MAX_SCAN_DURATION_MS,
          maxBuffer: 8 * 1024 * 1024,
        },
      );
      xml = result.stdout;
    } catch (error) {
      const code = (error as NodeJS.ErrnoException).code;

      if (code === 'ENOENT') {
        throw new ServiceUnavailableException(
          'Nmap is not installed on the backend host.',
        );
      }

      if (code === 'ETIMEDOUT') {
        throw new GatewayTimeoutException('The network scan timed out.');
      }

      throw new BadGatewayException('Nmap could not complete the network scan.');
    }

    const output = this.parser.parse(xml) as NmapOutput;
    const rawHosts = output.nmaprun?.host;
    const hosts = rawHosts == null
      ? []
      : Array.isArray(rawHosts)
        ? rawHosts
        : [rawHosts];

    const devices = hosts
      .filter((host) => host.status?.state === 'up')
      .map((host) => this.toDevice(host))
      .filter((device): device is NetworkDevice => device !== null);

    return {
      network,
      scannedAt: new Date().toISOString(),
      devices,
    };
  }

  private validateNetwork(network: string | undefined): asserts network is string {
    const ipv4Cidr = /^(?:(?:25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(?:25[0-5]|2[0-4]\d|1?\d?\d)\/(?:[0-9]|[12]\d|3[0-2])$/;

    if (!network || !ipv4Cidr.test(network)) {
      throw new ServiceUnavailableException(
        'Set LAN_SCAN_CIDR in backend/.env to a valid IPv4 subnet before scanning.',
      );
    }

    const prefixLength = Number(network.split('/')[1]);
    if (prefixLength < 20) {
      throw new ServiceUnavailableException(
        'LAN_SCAN_CIDR must use a /20 or narrower subnet to limit scan size.',
      );
    }
  }

  private toDevice(host: NmapHost): NetworkDevice | null {
    const addresses = host.address == null
      ? []
      : Array.isArray(host.address)
        ? host.address
        : [host.address];

    const ipv4 = addresses.find((address) => address.addrtype === 'ipv4');
    if (!ipv4?.addr) return null;

    const mac = addresses.find((address) => address.addrtype === 'mac');
    const hostname = host.hostnames?.hostname;
    const firstHostname = Array.isArray(hostname) ? hostname[0] : hostname;
    const vendor = mac?.vendor || 'Unknown';

    return {
      id: mac?.addr || ipv4.addr,
      hostname: firstHostname?.name || 'Unknown',
      ipv4: ipv4.addr,
      mac: mac?.addr || 'Unknown',
      vendor,
      type: this.guessDeviceType(vendor),
      status: 'ONLINE',
    };
  }

  private guessDeviceType(vendor: string): string {
    const value = vendor.toLowerCase();

    if (value.includes('raspberry')) return 'Raspberry Pi';
    if (value.includes('intel') || value.includes('realtek')) return 'PC';
    if (value.includes('tp-link')) return 'IoT / Router';
    if (value.includes('samsung')) return 'Smartphone';

    return 'Network Device';
  }
}
