import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';


interface GithubRelease {
  id: number;
  tag_name: string;
  name: string;
  prerelease: boolean;
  draft: boolean;
}

export const BONANZA_RELEASES_URL =
  'https://api.github.com/repos/johnny9/ESP-Miner-Bonanza/releases';

@Injectable({
  providedIn: 'root'
})
export class GithubUpdateService {

  constructor(
    private httpClient: HttpClient
  ) { }


  public getReleases(): Observable<GithubRelease[]> {
    return this.httpClient.get<GithubRelease[]>(
      BONANZA_RELEASES_URL
    ).pipe(
      /* Bonanza releases are intentionally pre-release while board 1002 is
       * still a prototype. GitHub returns newest first; exclude only drafts. */
      map((releases: GithubRelease[]) =>
        releases.filter((release: GithubRelease) => !release.draft))
    );
  }

}
