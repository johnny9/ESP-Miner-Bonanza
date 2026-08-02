import { TestBed } from '@angular/core/testing';

import {
  BONANZA_RELEASES_URL,
  GithubUpdateService
} from './github-update.service';
import { provideHttpClient } from '@angular/common/http';
import {
  HttpTestingController,
  provideHttpClientTesting
} from '@angular/common/http/testing';

describe('GithubUpdateService', () => {
  let service: GithubUpdateService;
  let http: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [provideHttpClient(), provideHttpClientTesting()]
    });
    service = TestBed.inject(GithubUpdateService);
    http = TestBed.inject(HttpTestingController);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });

  it('uses Bonanza releases and retains published beta builds', () => {
    let releases: any[] = [];
    service.getReleases().subscribe(value => releases = value);

    const request = http.expectOne(BONANZA_RELEASES_URL);
    expect(request.request.method).toBe('GET');
    request.flush([
      { id: 2, tag_name: 'v0.2.0-beta.1', name: 'beta', prerelease: true, draft: false },
      { id: 1, tag_name: 'v0.1.0', name: 'stable', prerelease: false, draft: false },
      { id: 3, tag_name: 'draft', name: 'draft', prerelease: true, draft: true },
    ]);

    expect(releases.map(release => release.tag_name)).toEqual([
      'v0.2.0-beta.1', 'v0.1.0'
    ]);
  });
});
