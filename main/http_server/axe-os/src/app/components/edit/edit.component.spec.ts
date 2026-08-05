import { ComponentFixture, TestBed } from '@angular/core/testing';

import { EditComponent } from './edit.component';
import { provideHttpClient } from '@angular/common/http';
import { provideToastr } from 'ngx-toastr';
import { provideRouter } from '@angular/router';
import { FormBuilder, FormControl, FormGroup } from '@angular/forms';
import { SystemAsic as ISystemASIC } from 'src/app/generated/models';
import { provideNoopAnimations } from '@angular/platform-browser/animations';

describe('EditComponent', () => {
  let component: EditComponent;
  let fixture: ComponentFixture<EditComponent>;

  const createDisplayForm = () => new FormGroup({
    frequency: new FormControl(800),
    coreVoltage: new FormControl(2800),
    autofanspeed: new FormControl(true),
    minFanSpeed: new FormControl(36),
    manualFanSpeed: new FormControl(70),
    temptarget: new FormControl(60),
    display: new FormControl('SSD1306 (128x32)'),
    rotation: new FormControl(0),
    displayTimeout: new FormControl(-1),
    invertscreen: new FormControl(false),
    statsFrequency: new FormControl(30),
  });

  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [EditComponent],
      providers: [provideHttpClient(), provideToastr(), provideRouter([]), provideNoopAnimations()]
    });
    fixture = TestBed.createComponent(EditComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });

  it('enables live BZM frequency and voltage settings', () => {
    const fb = TestBed.inject(FormBuilder);
    component.form = fb.group({
      frequency: [800],
      coreVoltage: [2800],
    });

    component.applyAsicCapabilities({
      ASICModel: 'BZM',
      deviceModel: 'Bonanza',
      swarmColor: 'yellow',
      asicCount: 4,
      defaultFrequency: 800,
      frequencyOptions: [800, 1000, 1250],
      frequencyTunable: true,
      defaultVoltage: 2800,
      voltageOptions: [2800, 2850, 2900, 3200],
      voltageTunable: true,
      fanSpeedMinimum: 36,
    } as ISystemASIC);

    expect(component.hasTunableAsicSettings).toBeTrue();
    expect(component.isBonanza).toBeTrue();
    expect(component.form.controls['frequency'].enabled).toBeTrue();
    expect(component.form.controls['coreVoltage'].enabled).toBeTrue();
    expect(component.noRestartFields).toContain('frequency');
    expect(component.noRestartFields).toContain('coreVoltage');
  });

  it('applies the hardware fan floor to live fan settings', () => {
    const fb = TestBed.inject(FormBuilder);
    component.form = fb.group({
      minFanSpeed: [0],
      manualFanSpeed: [10],
    });

    component.applyAsicCapabilities({
      fanSpeedMinimum: 36,
    } as ISystemASIC);

    expect(component.fanSpeedMinimum).toBe(36);
    expect(component.form.controls['minFanSpeed'].value).toBe(36);
    expect(component.form.controls['manualFanSpeed'].value).toBe(36);
    expect(component.noRestartFields).toContain('minFanSpeed');
  });

  it('shows external display information instead of local panel controls', () => {
    component.form = createDisplayForm();
    component.externalDisplay = true;
    component.externalDisplayConnected = true;

    fixture.detectChanges();

    const text = fixture.nativeElement.textContent;
    expect(text).toContain('External bonanzaDisplay: connected');
    expect(text).not.toContain('Invert Display Colors');
  });

  it('reports when the external display is not connected', () => {
    component.form = createDisplayForm();
    component.externalDisplay = true;
    component.externalDisplayConnected = false;

    fixture.detectChanges();

    expect(fixture.nativeElement.textContent)
      .toContain('External bonanzaDisplay: not connected');
  });
});
