import { ComponentFixture, TestBed, fakeAsync, tick } from '@angular/core/testing';
import { HttpErrorResponse, HttpResponse, provideHttpClient } from '@angular/common/http';
import { BehaviorSubject, of } from 'rxjs';

import { UpdateComponent } from './update.component';
import { ModalComponent } from '../modal/modal.component';
import { provideToastr, ToastrService } from 'ngx-toastr';
import { LiveDataService } from 'src/app/services/live-data.service';
import { SystemApiService } from 'src/app/services/system.service';
import {
  BridgeInfo, BridgeUpdateStatus, SystemInfo
} from 'src/app/generated/models';
import { CheckboxComponent } from '../checkbox/checkbox.component';
import { FormsModule } from '@angular/forms';
import { NoopAnimationsModule } from '@angular/platform-browser/animations';
import { ProgressbarComponent } from '../progressbar/progressbar.component';
import { getHttpErrorMessage } from 'src/app/utils/error-handler';

describe('UpdateComponent', () => {
  let component: UpdateComponent;
  let fixture: ComponentFixture<UpdateComponent>;
  let info: BehaviorSubject<SystemInfo>;
  let systemService: jasmine.SpyObj<SystemApiService>;

  const bridgeInfo: BridgeInfo = {
    available: true,
    versionQuerySupported: true,
    version: '1.2.3+gabcdef',
    protocolMajor: 1,
    protocolMinor: 0,
  };

  beforeEach(() => {
    info = new BehaviorSubject({ ASICModel: 'BZM' } as SystemInfo);
    systemService = jasmine.createSpyObj<SystemApiService>(
      'SystemApiService', [
        'getBridgeInfo',
        'performBridgeUpdate',
        'getBridgeFirmwareUpdateStatus',
        'performOTAUpdate',
        'performWWWOTAUpdate',
      ]);
    systemService.getBridgeInfo.and.returnValue(of(bridgeInfo));

    TestBed.configureTestingModule({
      declarations: [UpdateComponent, ModalComponent],
      imports: [CheckboxComponent, ProgressbarComponent, FormsModule, NoopAnimationsModule],
      providers: [
        provideHttpClient(),
        provideToastr(),
        { provide: LiveDataService, useValue: {
          info$: info.asObservable(),
          connected$: of(false),
        } },
        { provide: SystemApiService, useValue: systemService },
      ]
    });
    fixture = TestBed.createComponent(UpdateComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });

  it('recognizes only manifested Bonanza bridge release assets', () => {
    expect(component.isBridgeFirmwareAsset(
      'bonanza-bridge-fw-0.0.1-beta.1.bin')).toBeTrue();
    expect(component.isBridgeFirmwareAsset('esp-miner.bin')).toBeFalse();
    expect(component.isBridgeFirmwareAsset('bridge.uf2')).toBeFalse();
  });

  it('shows bridge version and uploader for BZM products', () => {
    fixture.detectChanges();
    const text = fixture.nativeElement.textContent;
    expect(text).toContain('Update Bridge Firmware');
    expect(text).toContain('Current Version: 1.2.3+gabcdef');
    expect(systemService.getBridgeInfo).toHaveBeenCalled();
  });

  it('does not expose bridge update controls on non-BZM products', () => {
    info.next({ ASICModel: 'BM1370' } as SystemInfo);
    fixture.detectChanges();
    expect(fixture.nativeElement.textContent)
      .not.toContain('Update Bridge Firmware');
  });

  it('rejects non-bin bridge uploads before making a request', () => {
    const toastr = TestBed.inject(ToastrService);
    spyOn(toastr, 'error');
    const file = new File(['invalid'], 'bridge.uf2');

    component.bridgeUpdate({
      target: { files: [file], value: 'bridge.uf2' }
    } as unknown as Event);

    expect(toastr.error).toHaveBeenCalled();
    expect(systemService.performBridgeUpdate).not.toHaveBeenCalled();
  });

  it('polls an accepted bridge update through version confirmation',
     fakeAsync(() => {
    const running: BridgeUpdateStatus = {
      state: 'preparing',
      progress: 0,
      imageSize: 1024,
      running: true,
      manifestValidated: true,
      forceRequested: false,
      targetBoardVersion: 1002,
      imageVersion: '1.2.4+g1234567',
      imageProtocolMajor: 1,
      imageProtocolMinor: 0,
      versionQuerySupported: false,
      currentVersion: null,
      error: null,
    };
    const complete: BridgeUpdateStatus = {
      ...running,
      state: 'complete',
      progress: 100,
      running: false,
      versionQuerySupported: true,
      currentVersion: '1.2.4+g1234567',
    };
    systemService.performBridgeUpdate.and.returnValue(of(
      new HttpResponse({ status: 202, body: running })));
    systemService.getBridgeFirmwareUpdateStatus.and.returnValue(of(complete));

    component.bridgeUpdate({ target: {
      files: [new File(['firmware'], 'bonanza-bridge-fw-1.2.4.bin')],
      value: 'bonanza-bridge-fw-1.2.4.bin'
    } } as unknown as Event);
    tick(0);

    expect(systemService.getBridgeFirmwareUpdateStatus).toHaveBeenCalled();
    expect(component.updateStatus).toBe('success');
    expect(component.updateMessage).toContain('1.2.4+g1234567');
  }));

  describe('getHttpErrorMessage', () => {
    it('should format HttpErrorResponse with status 0 as network error', () => {
      const err = new HttpErrorResponse({ status: 0, statusText: 'Unknown Error' });
      const msg = getHttpErrorMessage(err);
      expect(msg).toBe('Network error or connection lost. The device may have restarted or disconnected.');
    });

    it('should format HttpErrorResponse with string error body', () => {
      const err = new HttpErrorResponse({ status: 500, error: 'Write Error' });
      const msg = getHttpErrorMessage(err);
      expect(msg).toBe('Write Error');
    });

    it('should format HttpErrorResponse with JSON error body containing message', () => {
      const err = new HttpErrorResponse({ status: 500, error: { message: 'Out of flash memory' } });
      const msg = getHttpErrorMessage(err);
      expect(msg).toBe('Out of flash memory');
    });

    it('should format HttpErrorResponse with ProgressEvent error body', () => {
      const progressEvent = new ProgressEvent('error');
      const err = new HttpErrorResponse({ status: 500, error: progressEvent, statusText: 'Server Error' });
      const msg = getHttpErrorMessage(err);
      expect(msg).toBe('Upload failed: network error or connection closed.');
    });

    it('should format generic Error object message', () => {
      const err = new Error('Disk full');
      const msg = getHttpErrorMessage(err);
      expect(msg).toBe('Disk full');
    });

    it('should return string directly', () => {
      const msg = getHttpErrorMessage('Custom direct string error');
      expect(msg).toBe('Custom direct string error');
    });

    it('should return fallback message for null/undefined/other types', () => {
      expect(getHttpErrorMessage(null)).toBe('An unknown error occurred.');
      expect(getHttpErrorMessage(undefined)).toBe('An unknown error occurred.');
      expect(getHttpErrorMessage(123)).toBe('An unknown error occurred.');
    });

    it('should append device URI if provided', () => {
      const err = new HttpErrorResponse({ status: 500, error: 'Write Error' });
      const msg = getHttpErrorMessage(err, '192.168.1.10');
      expect(msg).toBe('Write Error (Device: 192.168.1.10)');
    });
  });
});
