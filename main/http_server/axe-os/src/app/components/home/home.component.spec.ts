import 'chartjs-adapter-moment';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { HomeComponent } from './home.component';
import { provideHttpClient } from '@angular/common/http';
import { provideToastr } from 'ngx-toastr';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { NoopAnimationsModule } from '@angular/platform-browser/animations';
import { Title } from '@angular/platform-browser';
import { provideRouter } from '@angular/router';
import { AppChartComponent } from 'src/app/components/chart/app-chart.component';
import { TooltipDirective } from 'src/app/directives/tooltip.directive';
import { DropdownComponent } from 'src/app/components/dropdown/dropdown.component';
import { BehaviorSubject, of } from 'rxjs';

import { HashSuffixPipe } from 'src/app/pipes/hash-suffix.pipe';
import { DiffSuffixPipe } from 'src/app/pipes/diff-suffix.pipe';
import { DateAgoPipe } from 'src/app/pipes/date-ago.pipe';
import { AddressPipe } from 'src/app/pipes/address.pipe';
import { SatsPipe } from 'src/app/pipes/sats.pipe';
import { ByteSuffixPipe } from 'src/app/pipes/byte-suffix.pipe';

import { TooltipTextIconComponent } from 'src/app/components/tooltip-text-icon/tooltip-text-icon.component';
import { TooltipIconComponent } from 'src/app/components/tooltip-icon/tooltip-icon.component';
import { ConfettiComponent } from 'src/app/components/confetti/confetti.component';
import { SnowflakesComponent } from 'src/app/components/snowflakes/snowflakes.component';

import { SystemApiService } from 'src/app/services/system.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { ThemeService } from 'src/app/services/theme.service';
import { QuicklinkService } from 'src/app/services/quicklink.service';
import { LoadingService } from 'src/app/services/loading.service';
import { ShareRejectionExplanationService } from 'src/app/services/share-rejection-explanation.service';
import { LocalStorageService } from 'src/app/local-storage.service';
import { DashboardEditService } from 'src/app/services/dashboard-edit.service';
import { LayoutService } from 'src/app/layout/service/app.layout.service';
import { SystemAsic as ISystemASIC, SystemInfo as ISystemInfo, SystemStatistics as ISystemStatistics } from 'src/app/generated/models';

const mockSystemInfo: ISystemInfo = {
  power_fault: '',
  blockFound: 0,
  sharesAccepted: 100,
  sharesRejected: 0,
  bestDiff: 1200000000,
  bestSessionDiff: 500000000,
  uptimeSeconds: 3600,
  hashRate: 500,
  hashRate_1m: 500,
  hashRate_10m: 500,
  hashRate_1h: 500,
  temp: 55,
  temp2: 0,
  vrTemp: 0,
  fanspeed: 80,
  fanrpm: 3000,
  fan2rpm: 0,
  power: 15,
  voltage: 5.0,
  nominalVoltage: 5.0,
  actualFrequency: 600,
  coreVoltageActual: 1.2,
  current: 0,
  coreVoltage: 0,
  maxPower: 20,
  poolConnectionInfo: 'Connected',
  responseTime: 45,
  responseShareBatch: 1,
  poolDifficulty: 1000,
  blockHeight: 800000,
  networkDifficulty: 5000000,
  scriptsig: 'test-scriptsig',
  coinbaseOutputs: [],
  coinbaseValueTotalSatoshis: 625000000,
  hashrateMonitor: {
    asics: [
      {
        errorCount: 0,
        domains: [500],
        total: 500
      }
    ]
  },
  showNewBlock: false,
  version: 'v2.1.2',
  boardVersion: 'v2.2',
  ssid: 'test-ssid',
  wifiStatus: 'Connected',
  wifiRSSI: -45,
  ipv4: '192.168.1.100',
  ipv6: 'fe80::1',
  macAddr: '00:11:22:33:44:55',
  cpuUsage: 12.5,
  freeHeap: 100000,
  freeHeapInternal: 50000,
  freeHeapSpiram: 50000,
  minFreeHeap: 40000,
  maxAllocHeap: 30000,
  axeOSVersion: 'v1.0.0',
  idfVersion: 'v5.1.0',
  stratumURL: 'stratum.pool.com',
  stratumUser: 'worker.name',
  stratumPort: 3333,
  stratumProtocol: 'SV1',
  fallbackStratumURL: 'fallback.pool.com',
  fallbackStratumUser: 'worker.fallback',
  fallbackStratumPort: 3333,
  fallbackStratumProtocol: 'SV1',
  isUsingFallbackStratum: false
} as any;

const mockSystemStatistics: ISystemStatistics = {
  labels: ['timestamp', 'hashrate', 'power'],
  statistics: [
    [Date.now() - 60000, 500, 15],
    [Date.now(), 500, 15]
  ],
  currentTimestamp: Date.now()
} as any;

const mockLiveDataService = {
  info$: new BehaviorSubject<ISystemInfo>(mockSystemInfo),
  connected$: of(true)
};

const mockSystemApiService = {
  getStatistics: () => of(mockSystemStatistics),
  getAsicSettings: () => of({
    ASICModel: 'BM1370',
    deviceModel: 'Gamma',
    swarmColor: 'purple',
    asicCount: 1,
    defaultFrequency: 485,
    frequencyOptions: [400, 485, 600],
    frequencyTunable: true,
    defaultVoltage: 1200,
    voltageOptions: [1100, 1200, 1300],
    voltageTunable: true,
    fanSpeedMinimum: 0,
  } as ISystemASIC),
  updateSystem: () => of(null),
  restart: () => of(null),
  dismissBlockFound: () => of(null)
};

const mockLocalStorageService = {
  getItem: () => null,
  setItem: () => {},
  getBool: () => false,
  setBool: () => {},
  getObject: () => null,
  setObject: () => {},
  getNumber: () => null,
  setNumber: () => {},
  removeItem: () => {}
};

describe('HomeComponent', () => {
  let component: HomeComponent;
  let fixture: ComponentFixture<HomeComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [
        HomeComponent,
        TooltipTextIconComponent,
        TooltipIconComponent,
        ConfettiComponent,
        SnowflakesComponent
      ],
      imports: [
        ReactiveFormsModule,
        FormsModule,
        NoopAnimationsModule,
        AppChartComponent,
        DropdownComponent,
        TooltipDirective,
        HashSuffixPipe,
        DiffSuffixPipe,
        DateAgoPipe,
        AddressPipe,
        SatsPipe,
        ByteSuffixPipe
      ],
      providers: [
        provideRouter([]),
        provideHttpClient(),
        provideToastr(),
        { provide: SystemApiService, useValue: mockSystemApiService },
        { provide: LiveDataService, useValue: mockLiveDataService },
        ThemeService,
        QuicklinkService,
        Title,
        LoadingService,
        ShareRejectionExplanationService,
        { provide: LocalStorageService, useValue: mockLocalStorageService },
        DashboardEditService,
        LayoutService
      ]
    });
    fixture = TestBed.createComponent(HomeComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });

  it('uses ASIC metadata for BZM warning and meter ranges', () => {
    component.configureAsicSettings({
      ASICModel: 'BZM',
      deviceModel: 'Bonanza',
      swarmColor: 'yellow',
      asicCount: 4,
      defaultFrequency: 800,
      frequencyOptions: [800, 1000, 1250],
      frequencyTunable: true,
      defaultVoltage: 2800,
      voltageOptions: [2800, 2900, 3200],
      voltageTunable: true,
    } as ISystemASIC);

    expect(component.minimumFrequency).toBe(800);
    expect(component.maxFrequency).toBe(1250);
    expect(component.maxCoreVoltage).toBe(3.2);

    const info = {
      frequency: 800,
      version: 'test',
      axeOSVersion: 'test',
    } as ISystemInfo;
    component.handleSystemMessages(info, { duration: 0, startTime: null });
    expect(component.messages.some(message => message.type === 'FREQUENCY_LOW')).toBeFalse();

    info.frequency = 799;
    component.handleSystemMessages(info, { duration: 0, startTime: null });
    expect(component.messages.some(message => message.type === 'FREQUENCY_LOW')).toBeTrue();
  });

  it('renders the production mining, pool, topology, power, bridge, and result health', () => {
    component.activePoolURL = 'pool.example';
    component.activePoolPort = 3333;
    component.activePoolProtocol = 'SV1';
    component.info$ = of({
      version: 'mvo-test',
      currentWorkAgeSeconds: 4.2,
      poolConnectionInfo: 'Connected',
      sharesAccepted: 7,
      sharesRejected: 1,
      hashRate: 710,
      hashRate_1m: 700,
      fanspeed: 100,
      fanrpm: 5200,
      fan2rpm: 0,
      asicHealth: {
        lifecycle: 'MINING',
        stateAgeSeconds: 125,
        asicCount: 4,
        expectedAsicCount: 4,
        activeEngineCount: 944,
        expectedEngineCount: 944,
        fixedFrequencyMHz: 800,
        fixedVoltageMV: 2800,
        measuredVoltageV: 2.8,
        boardTemperatureC: 64.5,
        fanPercent: 100,
        fanRPM: 5200,
        bridgeVersion: '0.0.1+mvo',
        bridgeProtocolMajor: 1,
        bridgeProtocolMinor: 0,
        bridgeCompatible: true,
        parserDiscardedBytes: 2,
        parserRecoveries: 1,
        bridgePioFifoOverflows: 0,
        bridgeSoftwareRingOverflows: 0,
        mappedResults: 240,
        locallyValidResults: 220,
        mappingRejections: 3,
        localRejections: 2,
        duplicateResults: 0,
        dispatchFailures: 0,
        lastFaultCode: 0,
        lastFault: '',
        automaticRetry: false,
        userActionRequired: false,
        recommendedAction: '',
      },
    } as ISystemInfo);

    fixture.detectChanges();

    const card = fixture.nativeElement.querySelector('[data-testid="asic-health"]') as HTMLElement;
    const text = card.textContent?.replace(/\s+/g, ' ') ?? '';
    expect(text).toContain('Miner health MINING');
    expect(text).toContain('pool.example:3333');
    expect(text).toContain('Work age: 4.2 s');
    expect(text).toContain('4 / 4 ASICs');
    expect(text).toContain('944 / 944 engines');
    expect(text).toContain('800 MHz at 2800 mV');
    expect(text).toContain('2.800 V measured');
    expect(text).toContain('protocol 1.0');
    expect(text).toContain('Mapped results: 240');
    expect(text).toContain('Locally valid: 220');
    expect(text).not.toContain('validation stage');
    expect(text).not.toContain('operator lease');
    expect(text).not.toContain('manual arm');

    const fanCard = fixture.nativeElement.querySelector('[gs-id="fan"]') as HTMLElement;
    const fanText = fanCard.textContent?.replace(/\s+/g, ' ') ?? '';
    expect(fanText).toContain('Fan Speed');
    expect(fanText).toContain('100.0 %');
  });

  it('should render the dashboard widgets and dropdowns when info is loaded', () => {
    fixture.detectChanges();
    const element = fixture.nativeElement;
    // Verify that the dropdowns inside *ngIf are rendered
    expect(element.querySelector('app-dropdown')).toBeTruthy();
  });

  describe('stale data and visibility state', () => {
    it('should set stale data error when visible and last message is old', () => {
      spyOnProperty(document, 'visibilityState', 'get').and.returnValue('visible');

      component['lastMessageTime'] = Date.now() - 10000;
      component.systemInfoError$.next({ duration: 0, startTime: null });

      component['checkStaleData']();

      expect(component.systemInfoError$.value.duration).toBe(10);
    });

    it('should NOT set stale data error when hidden and last message is old', () => {
      spyOnProperty(document, 'visibilityState', 'get').and.returnValue('hidden');

      component['lastMessageTime'] = Date.now() - 10000;
      component.systemInfoError$.next({ duration: 0, startTime: null });

      component['checkStaleData']();

      expect(component.systemInfoError$.value.duration).toBe(0);
    });

    it('should reset lastMessageTime and clear stale error when transitioning to visible', () => {
      spyOnProperty(document, 'visibilityState', 'get').and.returnValue('visible');

      const initialTime = Date.now() - 10000;
      component['lastMessageTime'] = initialTime;
      component.systemInfoError$.next({ duration: 10, startTime: initialTime });

      component.onVisibilityChange();

      expect(component.systemInfoError$.value.duration).toBe(0);
      expect(component.systemInfoError$.value.startTime).toBeNull();
      expect(component['lastMessageTime']).toBeGreaterThan(initialTime);
    });
  });
});
