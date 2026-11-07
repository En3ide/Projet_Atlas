import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { BadRequestException, Injectable } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { networkInterfaces } from 'node:os';

const execFileAsync = promisify(execFile);

@Injectable()
export class NetworkService {

    constructor(private readonly configService: ConfigService) {}

    /**
     * Cette fonction lance notre scanner ARP pour récupérer les appareils d'une plage du réseau
     *
     * @param interfaceName Le nom de l'interface à utiliser
     * @param cidr La plage IPv4 à scanner, par exemple 10.42.0.0/24
     * @returns La plage interrogée et les IP et MAC des appareils qui ont répondu
     */
    async scanNetwork(interfaceName: string, cidr: string) {
        // On vérifie que l'interface demandée existe sur le serveur
        const selectedInterface = this.getInterfaces().find(
            (networkInterface) => networkInterface.name === interfaceName,
        );

        if (!selectedInterface) {
            throw new BadRequestException(
                "L'interface réseau demandée est introuvable.",
            );
        }

        // On utilise le scanner installé par le script de déploiement
        const executable = this.configService.get<string>('ATLAS_ARP_BINARY')
            ?? '/usr/local/lib/atlas/atlas-arp';

        // On laisse le temps au scanner de parcourir la plage et de recevoir les réponses
        const { stdout } = await execFileAsync(
            executable,
            [interfaceName, cidr],
            {
                timeout: 65_000,
                maxBuffer: 1024 * 1024,
            },
        );
        return JSON.parse(stdout);
    }

    /**
     * Cette fonction récupère les interfaces IPv4 du serveur en laissant de côté les interfaces internes
     *
     * @returns Les interfaces disponibles avec leur IP, leur MAC, leur masque et leur CIDR
     */
    getInterfaces() {
        return Object.entries(networkInterfaces()).flatMap(
            ([name, addresses]) =>
                (addresses ?? [])
                .filter(
                    (address) =>
                        address.family === 'IPv4' && !address.internal,
                )
                .map((address) => ({
                    name,
                    address: address.address,
                    mac: address.mac,
                    netmask: address.netmask,
                    cidr: address.cidr,
                })),
        );
    }
}
