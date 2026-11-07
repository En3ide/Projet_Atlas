import { Test, TestingModule } from '@nestjs/testing';
import { AppController } from './app.controller';
import { AppService } from './app.service';

describe('AppController', () => {
  let appController: AppController;

  beforeEach(async () => {
    const app: TestingModule = await Test.createTestingModule({
      controllers: [AppController],
      providers: [AppService],
    }).compile();

    appController = app.get<AppController>(AppController);
  });

  describe('health', () => {
    it('retourne le statut du serveur et une date valide', () => {
      const response = appController.getHealth();

      expect(response.status).toBe('ok');
      expect(new Date(response.timestamp).toISOString()).toBe(response.timestamp);
    });
  });
});
