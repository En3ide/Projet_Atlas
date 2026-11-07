import {
    Body,
    Controller,
    Get,
    HttpCode,
    HttpStatus,
    Post,
} from '@nestjs/common';
import { NetworkService } from './network.service';
import { ScanNetworkDto } from './dto/scan-network.dto';

@Controller('network')
export class NetworkController {
    constructor(
        private readonly networkService: NetworkService,
    ) {}

    @Get('interfaces')
    getInterfaces() {
        return this.networkService.getInterfaces();
    }

    /**
     * Cette fonction lance notre scanner ARP sur une plage du réseau
     *
     * @param body L'interface à utiliser et la plage à scanner
     * @returns La plage interrogée et les appareils qui ont répondu
     */
    @Post('scans')
    @HttpCode(HttpStatus.OK)
    scanNetwork(@Body() body: ScanNetworkDto) {
        return this.networkService.scanNetwork(
            body.interfaceName,
            body.cidr,
        );
    }
}
