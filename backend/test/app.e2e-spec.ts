import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication, ValidationPipe } from '@nestjs/common';
import request from 'supertest';
import type { App } from 'supertest/types';
import { AppModule } from './../src/app.module';
import { NetworkService } from './../src/network/network.service';

describe('Atlas API (e2e)', () => {
  let app: INestApplication<App>;
  const networkService = {
    getInterfaces: jest.fn().mockReturnValue([
      {
        name: 'eth0',
        address: '192.168.1.10',
        mac: '02:11:22:33:44:55',
        netmask: '255.255.255.0',
        cidr: '192.168.1.10/24',
      },
    ]),
    scanNetwork: jest.fn().mockResolvedValue({
      network: '192.168.1.0/24',
      devices: [],
    }),
  };

  beforeAll(async () => {
    const moduleFixture: TestingModule = await Test.createTestingModule({
      imports: [AppModule],
    })
      .overrideProvider(NetworkService)
      .useValue(networkService)
      .compile();

    app = moduleFixture.createNestApplication();
    app.setGlobalPrefix('api');
    app.useGlobalPipes(
      new ValidationPipe({
        transform: true,
        whitelist: true,
        forbidNonWhitelisted: true,
      }),
    );
    await app.init();
  });

  beforeEach(() => {
    jest.clearAllMocks();
  });

  it('GET /api/health retourne le statut du serveur', () => {
    return request(app.getHttpServer())
      .get('/api/health')
      .expect(200)
      .expect(({ body }) => {
        expect(body.status).toBe('ok');
        expect(new Date(body.timestamp).toISOString()).toBe(body.timestamp);
      });
  });

  it('GET /api/network/interfaces retourne les interfaces', async () => {
    const response = await request(app.getHttpServer())
      .get('/api/network/interfaces')
      .expect(200);

    expect(response.body[0].name).toBe('eth0');
    expect(response.body[0].mac).toBe('02:11:22:33:44:55');
    expect(response.body[0].netmask).toBe('255.255.255.0');
  });

  it('POST /api/network/scans transmet le CIDR et accepte une liste vide', async () => {
    const response = await request(app.getHttpServer())
      .post('/api/network/scans')
      .send({ interfaceName: 'eth0', cidr: '192.168.1.0/24' })
      .expect(200);

    expect(networkService.scanNetwork).toHaveBeenCalledWith('eth0', '192.168.1.0/24');
    expect(response.body).toEqual({ network: '192.168.1.0/24', devices: [] });
  });

  it.each([
    { interfaceName: 'eth0', cidr: '192.168.1.0/19' },
    { interfaceName: 'eth0', cidr: '999.168.1.0/24' },
    { interfaceName: 'eth0', ipAddress: '192.168.1.1' },
    { interfaceName: 'eth0', cidr: '192.168.1.0/24', extra: true },
  ])('refuse une requête de scan invalide sans appeler le scanner : %j', async (body) => {
    await request(app.getHttpServer())
      .post('/api/network/scans')
      .send(body)
      .expect(400);

    expect(networkService.scanNetwork).not.toHaveBeenCalled();
  });

  afterAll(async () => {
    await app.close();
  });
});
