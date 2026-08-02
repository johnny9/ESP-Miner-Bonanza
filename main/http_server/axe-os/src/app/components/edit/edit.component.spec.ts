import { ComponentFixture, TestBed } from '@angular/core/testing';

import { EditComponent } from './edit.component';
import { provideHttpClient } from '@angular/common/http';
import { provideToastr } from 'ngx-toastr';
import { provideRouter } from '@angular/router';
import { FormBuilder } from '@angular/forms';
import { SystemAsic as ISystemASIC } from 'src/app/generated/models';

describe('EditComponent', () => {
  let component: EditComponent;
  let fixture: ComponentFixture<EditComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [EditComponent],
      providers: [provideHttpClient(), provideToastr(), provideRouter([])]
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
});
