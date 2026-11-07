import { IsIP, IsString, Length } from 'class-validator';

/**
 * les informations nécessaires pour interroger une machine du réseau
 */
export class ScanHostDto {
    @IsString()
    @Length(1, 15)
    interfaceName: string;

    @IsIP('4')
    ipAddress: string;
}