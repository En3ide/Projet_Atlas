import { Module } from '@nestjs/common';
import { ConfigModule } from '@nestjs/config';
import { AppController } from './app.controller';
import { AppService } from './app.service';
import { DevicesModule } from './devices/devices.module';
import { NetworkModule } from './network/network.module';

@Module({
  imports: [ConfigModule.forRoot({ isGlobal: true }), DevicesModule, NetworkModule],
  controllers: [AppController],
  providers: [AppService],
})
export class AppModule {}
