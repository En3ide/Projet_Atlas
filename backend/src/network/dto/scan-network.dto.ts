import { IsString, Length, Matches } from 'class-validator';
/**
 * Les informations nécessaires pour scanner une plage du réseau
 */
export class ScanNetworkDto {
    @IsString()
    @Length(1, 15)
    interfaceName: string;

    @IsString()
    @Matches(
        /^(?:(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)\.){3}(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)\/(?:2\d|3[0-2])$/,
        { message: 'La plage doit être une IPv4 avec un préfixe entre /20 et /32.' },
    )
    cidr: string;
}