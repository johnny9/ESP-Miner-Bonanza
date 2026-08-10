import { Component, ElementRef, OnDestroy, ViewChild } from '@angular/core';
import {
  Observable, Subscription, catchError, map, of, shareReplay,
  switchMap, takeWhile, timer
} from 'rxjs';
import { HttpErrorResponse, HttpEventType } from '@angular/common/http';
import { getHttpErrorMessage } from 'src/app/utils/error-handler';
import { ToastrService } from 'ngx-toastr';
import { GithubUpdateService } from 'src/app/services/github-update.service';
import { SystemApiService } from 'src/app/services/system.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { LocalStorageService } from 'src/app/local-storage.service';
import { ModalComponent } from '../modal/modal.component';
import {
  BridgeInfo, BridgeUpdateStatus, SystemInfo
} from 'src/app/generated/models';

const IGNORE_RELEASE_CHECK_WARNING = 'IGNORE_RELEASE_CHECK_WARNING';

@Component({
    selector: 'app-update',
    templateUrl: './update.component.html',
    styleUrls: ['./update.component.scss'],
    standalone: false
})
export class UpdateComponent implements OnDestroy {

  public firmwareUpdateProgress: number = 0;
  public websiteUpdateProgress: number = 0;
  public bridgeUpdateProgress: number = 0;
  public bridgeUpdatePhase: string = '';

  public checkLatestRelease: boolean = false;
  public latestRelease$: Observable<any>;

  public info$: Observable<SystemInfo>;
  public bridgeInfo$: Observable<BridgeInfo | null>;

  @ViewChild('firmwareUpload') firmwareUpload!: ElementRef<HTMLInputElement>;
  @ViewChild('websiteUpload') websiteUpload!: ElementRef<HTMLInputElement>;
  @ViewChild('bridgeUpload') bridgeUpload?: ElementRef<HTMLInputElement>;

  @ViewChild('privacyModal') privacyModal?: ModalComponent;
  @ViewChild('progressModal') progressModal?: ModalComponent;

  public updateTarget: string = '';
  public updateStatus: 'progress' | 'success' | 'error' = 'progress';
  public updateMessage: string = '';
  private bridgePollSubscription?: Subscription;

  private currentVersion: string | undefined = undefined;

  constructor(
    private systemService: SystemApiService,
    private liveDataService: LiveDataService,
    private toastrService: ToastrService,
    private githubUpdateService: GithubUpdateService,
    private localStorageService: LocalStorageService,
  ) {
    this.latestRelease$ = this.githubUpdateService.getReleases().pipe(map(releases => {
      return (releases as any)[0];
    }));

    this.info$ = this.liveDataService.info$;
    this.bridgeInfo$ = this.info$.pipe(
      switchMap(info => info.ASICModel === 'BZM'
        ? this.systemService.getBridgeInfo().pipe(catchError(() => of({
            available: false,
            versionQuerySupported: false,
            version: null,
            protocolMajor: null,
            protocolMinor: null,
          })))
        : of(null)),
      shareReplay({ bufferSize: 1, refCount: true }),
    );

    // Reload page if firmware version changes
    this.liveDataService.info$.subscribe(info => {
      if (this.currentVersion === undefined) {
        this.currentVersion = info.version;
      } else if (info.version !== this.currentVersion) {
        window.location.reload();
      }
    });

    // Reload page when device comes back online after a successful update
    this.liveDataService.connected$.subscribe(connected => {
      if (connected && this.updateStatus === 'success') {
        window.location.reload();
      }
    });
  }

  ngOnDestroy(): void {
    this.bridgePollSubscription?.unsubscribe();
  }

  public get modalProgress(): number {
    if (this.updateTarget === 'AxeOS') return this.websiteUpdateProgress;
    if (this.updateTarget === 'Bridge') return this.bridgeUpdateProgress;
    return this.firmwareUpdateProgress;
  }

  public isBridgeFirmwareAsset(name: string): boolean {
    return /^bonanza-bridge-fw-.*\.bin$/i.test(name);
  }

  onFileSelected(event: Event, target: 'websiteUpload' | 'firmwareUpload') {
    const input = event.target as HTMLInputElement;
    if (input.files && input.files.length > 0) {
      const file = input.files[0];
      if (target === 'websiteUpload') {
        this.otaWWWUpdate(file);
      } else {
        this.otaUpdate(file);
      }
    }
  }

  otaUpdate(file: File) {
    if (this.firmwareUpload) {
      this.firmwareUpload.nativeElement.value = '';
    }

    if (file.name != 'esp-miner.bin') {
      this.toastrService.error('Incorrect file, looking for esp-miner.bin.');
      return;
    }

    this.updateTarget = 'Firmware';
    this.updateStatus = 'progress';
    this.updateMessage = '';
    if (this.progressModal) {
      this.progressModal.isVisible = true;
    }

    this.systemService.performOTAUpdate(file)
      .subscribe({
        next: (event: any) => {
          if (event.type === HttpEventType.UploadProgress) {
            this.firmwareUpdateProgress = Math.round((event.loaded / (event.total as number)) * 100);
          } else if (event.type === HttpEventType.Response) {
            if (event.ok) {
              this.updateStatus = 'success';
              this.updateMessage = 'Firmware updated. The page will reload when the device comes back online.';
            } else {
              this.updateStatus = 'error';
              this.updateMessage = event.statusText || 'An unknown error occurred.';
            }
          }
          else if (event instanceof HttpErrorResponse)
          {
            this.updateStatus = 'error';
            this.updateMessage = getHttpErrorMessage(event);
          }
        },
        error: (err) => {
          this.updateStatus = 'error';
          this.updateMessage = getHttpErrorMessage(err);
        },
        complete: () => {
          this.firmwareUpdateProgress = 0;
        }
      });
  }

  otaWWWUpdate(file: File) {
    if (this.websiteUpload) {
      this.websiteUpload.nativeElement.value = '';
    }

    if (file.name != 'www.bin') {
      this.toastrService.error('Incorrect file, looking for www.bin.');
      return;
    }

    this.updateTarget = 'AxeOS';
    this.updateStatus = 'progress';
    this.updateMessage = '';
    if (this.progressModal) {
      this.progressModal.isVisible = true;
    }

    this.systemService.performWWWOTAUpdate(file)
      .subscribe({
        next: (event: any) => {
          if (event.type === HttpEventType.UploadProgress) {
            this.websiteUpdateProgress = Math.round((event.loaded / (event.total as number)) * 100);
          } else if (event.type === HttpEventType.Response) {
            if (event.ok) {
              this.updateStatus = 'success';
              this.updateMessage = 'AxeOS updated. The page will reload when the device comes back online.';
            } else {
              this.updateStatus = 'error';
              this.updateMessage = event.statusText || 'An unknown error occurred.';
            }
          }
          else if (event instanceof HttpErrorResponse)
          {
            this.updateStatus = 'error';
            this.updateMessage = getHttpErrorMessage(event);
          }
        },
        error: (err) => {
          this.updateStatus = 'error';
          this.updateMessage = getHttpErrorMessage(err);
        },
        complete: () => {
          this.websiteUpdateProgress = 0;
        }
      });
  }

  bridgeUpdate(event: Event) {
    const input = event.target as HTMLInputElement;
    const file = input.files?.[0];
    input.value = '';

    if (!file || !this.isBridgeFirmwareAsset(file.name)) {
      this.toastrService.error(
        'Incorrect file, select a manifested bonanza-bridge-fw-*.bin image.');
      return;
    }

    this.updateTarget = 'Bridge';
    this.updateStatus = 'progress';
    this.updateMessage = '';
    this.bridgeUpdateProgress = 0;
    this.bridgeUpdatePhase = 'uploading';
    if (this.progressModal) this.progressModal.isVisible = true;

    this.systemService.performBridgeUpdate(file).subscribe({
      next: event => {
        if (event.type === HttpEventType.UploadProgress) {
          this.bridgeUpdateProgress = event.total
            ? Math.round(event.loaded * 100 / event.total)
            : 0;
        } else if (event.type === HttpEventType.Response) {
          this.applyBridgeStatus(event.body);
          this.pollBridgeUpdate();
        }
      },
      error: err => {
        this.updateStatus = 'error';
        this.updateMessage = err.error?.message || err.error ||
          err.message || 'Bridge firmware upload failed';
      },
    });
  }

  private pollBridgeUpdate(): void {
    this.bridgePollSubscription?.unsubscribe();
    this.bridgePollSubscription = timer(0, 750).pipe(
      switchMap(() => this.systemService.getBridgeFirmwareUpdateStatus()),
      takeWhile(status => status.running, true),
    ).subscribe({
      next: status => this.applyBridgeStatus(status),
      error: err => {
        this.updateStatus = 'error';
        this.updateMessage = err.error?.message || err.error ||
          err.message || 'Unable to read bridge update status';
      },
    });
  }

  private applyBridgeStatus(status: BridgeUpdateStatus | null): void {
    if (!status) return;
    this.bridgeUpdatePhase = status.state;
    this.bridgeUpdateProgress = status.progress;
    if (status.state === 'complete') {
      this.updateStatus = 'success';
      this.updateMessage = status.currentVersion
        ? `Bridge firmware updated to ${status.currentVersion}. Restart ESP-Miner before mining.`
        : 'Bridge firmware updated successfully. Restart ESP-Miner before mining.';
      this.bridgeInfo$ = this.systemService.getBridgeInfo().pipe(
        catchError(() => of({
          available: true,
          versionQuerySupported: false,
          version: null,
          protocolMajor: null,
          protocolMinor: null,
        })),
      );
    } else if (status.state === 'failed') {
      this.updateStatus = 'error';
      this.updateMessage = status.error || 'Bridge firmware update failed';
    }
  }

  // https://gist.github.com/elfefe/ef08e583e276e7617cd316ba2382fc40
  public simpleMarkdownParser(markdown: string): string {
    const toHTML = markdown
      .replace(/^#{1,6}\s+(.+)$/gim, '<h4 class="mt-2">$1</h4>') // Headlines
      .replace(/\*\*(.+?)\*\*|__(.+?)__/gim, '<b>$1</b>') // Bold text
      .replace(/\*(.+?)\*|_(.+?)_/gim, '<i>$1</i>') // Italic text
      .replace(/\[(.*?)\]\((.*?)\s?(?:"(.*?)")?\)/gm, '<a href="$2" class="underline text-white" target="_blank">$1</a>') // Markdown links
      .replace(/(https?:\/\/github\.com\/.+\/(.+[^\s])+)/gim, (match, p1, p2) => `<a href="${p1}" target="_blank">${match.includes('/pull/') ? '#' : ''}${p2}</a>`) // Regular links
      .replace(/@([^\s]+)/gim, ' <a href="https://github.com/$1" target="_blank">@$1</a> ') // Username links
      .replace(/^\s*[-+*]\s?(.+)$/gim, '<li>$1</li>') // Unordered list
      .replace(/`([^`]+)`/gim, '<code class="bg-surface-100 rounded px-1">$1</code>') // Code
      .replace(/\r\n\r\n/gim, '<br>'); // Breaks

    return toHTML.trim();
  }

  public handleReleaseCheck(): void {
    if (this.localStorageService.getBool(IGNORE_RELEASE_CHECK_WARNING)) {
      this.checkLatestRelease = true;
    } else {
      if (this.privacyModal) {
        this.privacyModal.isVisible = true;
      }
    }
  }

  public continueReleaseCheck(skipWarning: boolean): void {
    this.checkLatestRelease = true;
    if (this.privacyModal) {
      this.privacyModal.isVisible = false;
    }

    if (!skipWarning) {
      return;
    }

    this.localStorageService.setBool(IGNORE_RELEASE_CHECK_WARNING, true);
  }

  public switchPartition(label: string): void {
    if (confirm(`Set ${label} as the next boot partition? The device will restart to apply this change.`)) {
      this.systemService.switchBootPartition(label).subscribe({
        next: (resp) => {
          this.toastrService.success(resp.message);
        },
        error: (err) => {
          this.toastrService.error(err.error?.message || err.message || 'Failed to switch partition');
        }
      });
    }
  }

  public restart(): void {
    if (confirm('Are you sure you want to restart the device?')) {
      this.systemService.restart().subscribe({
        next: () => {
          this.toastrService.success('Restart command sent.');
        },
        error: (err) => {
          this.toastrService.error(err.error?.message || err.message || 'Failed to restart device');
        }
      });
    }
  }

  public toggleCustomWWW(checked: boolean): void {
    const value = checked ? 1 : 0;
    this.systemService.updateSystem('', { useCustomWWW: value }).subscribe({
      next: () => {
        this.toastrService.success(
          `Web UI source changed to ${checked ? 'Custom' : 'Embedded'}. A device restart is required to apply the change.`,
          'Setting Saved',
          { timeOut: 8000 }
        );
      },
      error: (err) => {
        this.toastrService.error(`Failed to change Web UI source. ${getHttpErrorMessage(err)}`);
      }
    });
  }
}
